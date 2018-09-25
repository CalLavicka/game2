#pragma once

#include <glm/glm.hpp>

#include "Connection.hpp"

using namespace glm;

struct Snake {

    enum Direction {
        UP = 0,
        LEFT = 1,
        DOWN = 2,
        RIGHT = 3
    };

    struct BodySegment {
        vec2 front;
        float length = 0.f;

        BodySegment * next = nullptr;
        BodySegment * prev = nullptr;

        int dir;

        int id;


        BodySegment(vec2 pos, int dir, int id);
        bool collides_with(vec2 pt, float radius);
        vec2 dir_vec();
    };

    BodySegment * head;
    BodySegment * tail;

    float speed = 6.f;
    float extra_length = 0.f;
    int dir;

    Snake(vec2 pos, float length, int dir);
    ~Snake();

    void update(float elapsed);
    void change_dir(int new_dir);
    vec2 revert_and_change(vec2 target_pos, int new_dir, float max_backtrack = FLT_MAX);
    vec2 dir_vec();
    float camera_angle();

    bool collision_with_self();
    bool collision_with_other(Snake *);

    bool dead = false;

    // Networking functions
    int serial_length();
    int serialize(void * target_buf);
    int deserialize(void * buf);
};