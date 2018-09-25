#include "Game.hpp"

#include <iostream>

using namespace glm;


const float Game::MAX_X = 10.f;
const float Game::MAX_Y = 10.f;

void Game::new_game(int players) {
	for(Snake *snake : snakes) {
		delete snake;
	}

	snakes.clear();

	for(int i=0; i<players; i++) {
		snakes.push_back(new Snake(vec2(i * 2.f, 0.f), 2.f, Snake::Direction::UP));
	}
}

bool Game::update(float time, bool server) {
	
	bool ret = false;
	for (Snake *snake : snakes) {
		if(snake->dead) {
			continue;
		}
		snake->update(time);

		vec2 dif = snake->head->front - apple_pos;
		if (!ret && dot(dif, dif) <= 1.f) {
			snake->extra_length += 1.f;
			ret = true;
		}

		if (server) {
			if (snake->collision_with_self()) {
				snake->dead = true;
				std::cout << "COLLISION" << std::endl;
				continue;
			}

			if (abs(snake->head->front.x) >= MAX_X || abs(snake->head->front.y) >= MAX_Y) {
				snake->dead = true;
				continue;
			}

			for (Snake *other : snakes) {
				if (other != snake && !other->dead && snake->collision_with_other(other)) {
					snake->dead = true;
					std::cout << "COLLISION2" << std::endl;
					break;
				}
			}
		}
	}

	return ret;
}

void Game::send_sync(std::list< Connection > &connections) {
	// Total size is size of all snakes + signal byte + length param
	int total_size = 1 + sizeof(int);
	for(Snake * snake : snakes) {
		total_size += snake->serial_length();
	}

	char * buf = (char *)malloc(total_size);

	buf[0] = 'y';
	memcpy(buf + 1, &total_size, sizeof(int));
	int offset = 1 + sizeof(int);

	for(Snake * snake : snakes) {
		offset += snake->serialize(buf + offset);
	}

	for (Connection &conn : connections) {
		conn.send_raw(buf, total_size);
	}

	assert(offset == total_size);
	free(buf);
}

void Game::recv_sync(Connection *conn) {
	assert(conn->recv_buffer[0] == 'y');
	if (conn->recv_buffer.size() < 5) {
		return;
	}

	int size;
	memcpy(&size, conn->recv_buffer.data() + 1, sizeof(int));
	if (conn->recv_buffer.size() >= size) {
		char *data = (char *)malloc(size);
		memcpy(data, conn->recv_buffer.data(), size);
		conn->recv_buffer.erase(conn->recv_buffer.begin(), conn->recv_buffer.begin() + size);

		int offset = 1 + sizeof(int);
		for(Snake * snake : snakes) {
			offset += snake->deserialize(data + offset);
		}
		
		assert(offset == size);
		free(data);
	}
}
