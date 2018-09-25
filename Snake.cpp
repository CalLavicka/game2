#include "Snake.hpp"

#include <stdio.h>
#include <glm/gtc/quaternion.hpp>

#include <iostream>

#define SNAKE_RADIUS 0.4f

using namespace glm;

const vec2 dir_to_vec[] = {
    vec2(0.f, 1.f), vec2(-1.f, 0.f), vec2(0.f, -1.f), vec2(1.f, 0.f)
};

#define PI 3.14159265f

const float dir_to_angle[] = {
    0.0f,
    PI / 2.f,
    PI,
    PI * 3.f / 2.f
};

vec2 Snake::dir_vec() {
    return dir_to_vec[dir];
}

float Snake::camera_angle() {
    return dir_to_angle[dir];
}

vec2 Snake::BodySegment::dir_vec() {
    return dir_to_vec[dir];
}

Snake::BodySegment::BodySegment(vec2 front, int dir, int id) {
    this->front = front;
    this->dir = dir;
    this->id = id;
}

float distance2(vec2 const v, vec2 const u) {
    vec2 d = v - u;
    return dot(d, d);
}

// Code modified slightly from
// https://stackoverflow.com/questions/849211/shortest-distance-between-a-point-and-a-line-segment
float minimum_distance_2(vec2 const v, vec2 const w, vec2 const p) {
  // Return minimum distance between line segment vw and point p
  const float l2 = distance2(v, w);  // i.e. |w-v|^2 -  avoid a sqrt
  if (l2 == 0.0) return distance2(p, v);   // v == w case
  // Consider the line extending the segment, parameterized as v + t (w - v).
  // We find projection of point p onto the line. 
  // It falls where t = [(p-v) . (w-v)] / |w-v|^2
  // We clamp t from [0,1] to handle points outside the segment vw.
  const float t = max(0.f, min(1.f, dot(p - v, w - v) / l2));
  const vec2 projection = v + t * (w - v);  // Projection falls on the segment
  return distance2(p, projection);
}

bool Snake::BodySegment::collides_with(vec2 pt, float radius) {
    vec2 back = front - dir_to_vec[dir] * length;
    float dist2 = minimum_distance_2(front, back, pt);
    float comb_radius = radius + SNAKE_RADIUS;
    return dist2 <= comb_radius * comb_radius;
}

Snake::Snake(vec2 pos, float length, int dir) {
    head = new BodySegment(pos, dir, 0);
    head->length = length;
    tail = head;
    this->dir = dir;
}

Snake::~Snake() {
    while(tail != head) {
        BodySegment * next = tail->next;
        delete tail;
        tail = next;
    }
    delete head;
}

void Snake::update(float elapsed) {
    // First, update head
    float dist = speed * elapsed;
    head->front += dir_to_vec[dir] * dist;
    head->length += dist;

    // Now, update tail
    extra_length -= dist;
    if (extra_length < 0) {
        dist = -extra_length;
        extra_length = 0;
        while(dist > 0) {
            if (tail->length > dist) {
                tail->length -= dist;
                break;
            } else {
                BodySegment * next = tail->next;
                dist -= tail->length;
                delete tail;
                tail = next;
                next->prev = nullptr;

            }
        }
    }
}

void Snake::change_dir(int new_dir) {
    BodySegment * new_seg = new BodySegment(head->front, new_dir, head->id + 1);

    new_seg->prev = head;
    head->next = new_seg;
    head = new_seg;

    this->dir = new_dir;
}

vec2 Snake::revert_and_change(vec2 target_pos, int new_dir, float max_backtrack) {
    float dist = length(head->front - target_pos);
    if (dist > max_backtrack) {
        dist = max_backtrack;
        head->front += (target_pos - head->front) / dist * max_backtrack;
    } else {
        head->front = target_pos;
    }
    head->length -= dist;

    vec2 ret = head->front;

    change_dir(new_dir);
    head->front += dist * dir_to_vec[new_dir];
    head->length = dist;

    // Return turn point
    return ret;
}

bool Snake::collision_with_self() {
    if (head->prev != nullptr && head->prev->prev != nullptr) {
        for(BodySegment *seg = head->prev->prev->prev; seg != nullptr; seg = seg->prev) {
            if (seg->collides_with(head->front, SNAKE_RADIUS)) {
                return true;
            }
        }
    }
    return false;
}

bool Snake::collision_with_other(Snake *other) {
    for(BodySegment *seg = other->tail; seg != nullptr; seg = seg->next) {
        if (seg->collides_with(head->front, SNAKE_RADIUS)) {
            return true;
        }
    }
    return false;
}

// Network functions

int Snake::serial_length() {
    // Each segment has direction, length, id, and front point
    int body_size = 1 + sizeof(int) + sizeof(float) + sizeof(float) + sizeof(float);
    int body_length = 1;
    for(BodySegment * node = tail; node != head; node = node->next) {
        body_length++;
    }

    // All the bodies, 1 for direction, float for extra_length, and int for this number itself
    return body_size * body_length + 1 + sizeof(float) + sizeof(int);
}

int Snake::serialize(void *target_buf) {
    char *buf = (char *)target_buf;
    int serial_size = serial_length();

    int offset = 0;
    auto send_data = [&offset, buf](void *data, int size) {
        memcpy(buf + offset, data, size);
        offset += size;
    };
    
    // First, send size of package
    send_data(&serial_size, sizeof(int));

    // Next, send direction
    char dir = (char) this->dir;
    send_data(&dir, 1);

    // Send extra_length
    send_data(&this->extra_length, sizeof(float));

    // Send all segments
    for (BodySegment *seg = tail; seg != nullptr; seg = seg->next) {
        send_data(&seg->id, sizeof(int));

        dir = (char)seg->dir;
        send_data(&dir, 1);

        send_data(&seg->front.x, sizeof(float));
        send_data(&seg->front.y, sizeof(float));
        send_data(&seg->length, sizeof(float));
    }

    return serial_size;
}

int Snake::deserialize(void *buf) {
    char *old_buf = (char *) buf;

    int offset = 0;
    auto recv_data = [&offset, old_buf](void *data, int size) {
        memcpy(data, old_buf + offset, size);
        offset += size;
    };

    // Get size
    int size;
    recv_data(&size, sizeof(int));

    // Get direction
    char dir;
    recv_data(&dir, 1);
    // Don't update because our info is correct
    //this->dir = (int) dir;

    // Get extra_length
    recv_data(&this->extra_length, sizeof(float));

    int id;

    // Get all segments, stop if exceding size (as server may not have all bodyparts we have)
    // May have more/less bodyparts on server, in which case update to those body parts
    for(BodySegment *seg = this->tail; offset < size; seg = seg->next) {
        recv_data(&id, sizeof(int));
        // check if directions line up
        if (seg->id - id > 0) {
            // New node the server has that we don't
            BodySegment * next = seg;
            seg = new BodySegment(vec2(), 0, id);
            seg->next = next;
            seg->prev = next->prev;
            if (next->prev != nullptr) {
                next->prev->next = seg;
            } else {
                this->tail = seg;
            }
            next->prev = seg;

        } else {
            while (seg->id - id < 0) {
                // our segment is extra
                if (seg->prev != nullptr) {
                    seg->prev->next = seg->next;
                } else {
                    this->tail = seg->next;
                }
                seg->next->prev = seg->prev;
                seg = seg->next;
                delete seg->prev;
            }
        }

        recv_data(&dir, 1);
        seg->dir = dir;
        recv_data(&seg->front.x, sizeof(float));
        recv_data(&seg->front.y, sizeof(float));
        recv_data(&seg->length, sizeof(float));

        (void)offset; // To shut the compiler up about modifying offset in the loop
    }

    //std::cout << "HEAD: " << this->head->id << ", SHOULD BE: " << id << std::endl;

    assert(offset == size);
    return size;
}