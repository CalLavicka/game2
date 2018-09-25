#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Snake.hpp"
#include "Connection.hpp"

struct Game {

	// Game logic functions
	void new_game(int players);
	bool update(float time, bool server);

	// Network functions
	void send_sync(std::list< Connection > &connections);
	void recv_sync(Connection *conn);

	static const int BOARD_WIDTH = 9;
	static const int BOARD_HEIGHT = 9;
	static const float MAX_X;
	static const float MAX_Y;

	glm::vec2 apple_pos;

	std::vector<Snake *> snakes;
};
