#include "Connection.hpp"
#include "Game.hpp"
#include "Snake.hpp"

#include <iostream>
#include <set>
#include <chrono>
#include <unordered_map>
#include <random>

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}
	
	Server server(argv[1]);

	std::random_device r;
	std::seed_seq seed{r(), r(), r(), r(), r(), r(), r(), r()};
	std::mt19937 rnd{seed};

	Game state;
	std::unordered_map<Connection *, char> players;
	char player_count = 0;
	int start_count = 2;

	auto initialize_game = [&state, &players, &player_count, &start_count, &rnd]() {
		state.new_game(2);

		state.apple_pos = vec2(((int)(rnd() % (2 * Game::BOARD_WIDTH + 1))) - Game::BOARD_WIDTH,
								((int)(rnd() % (2 * Game::BOARD_HEIGHT + 1))) - Game::BOARD_HEIGHT);
		std::cout << "Initial apple: " << state.apple_pos.x << ", " << state.apple_pos.y << std::endl;

		player_count = 0;
		start_count = 2;
		players.clear();
	};
	initialize_game();

	while (1) {
		server.poll([&](Connection *c, Connection::Event evt){
			if (evt == Connection::OnOpen) {
				if (player_count <= 2) {
					c->send_raw("p\2", 2);
					c->send_raw(&player_count, 1);
					c->send_raw(&state.apple_pos.x, sizeof(float));
					c->send_raw(&state.apple_pos.y, sizeof(float));
					players.insert(std::make_pair(c, player_count));
					player_count++;
				}
			} else if (evt == Connection::OnClose) {
				initialize_game();
			} else { assert(evt == Connection::OnRecv);
				if (c->recv_buffer[0] == 'h') {
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
					std::cout << c << ": Got hello." << std::endl;
					start_count--;

					if (start_count == 0) {
						std::cout << "Starting game" << std::endl;
						for(Connection &other : server.connections) {
							other.send_raw("s", 1);
						}
					}
				} else if (c->recv_buffer[0] == 'm') {
					if (c->recv_buffer.size() < 2 + sizeof(float) + sizeof(float)) {
						return; //wait for more data
					} else {
						char dir = c->recv_buffer[1];
						vec2 target;
						memcpy(&target.x, c->recv_buffer.data() + 2, sizeof(float));
						memcpy(&target.y, c->recv_buffer.data() + 2 + sizeof(float), sizeof(float));
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 2 + sizeof(float) + sizeof(float));
					
						auto player_num = players.find(c);
						if (player_num != players.end()) {
							char player = player_num->second;
							target = state.snakes[player]->revert_and_change(target, dir, 2.f);

							for(Connection &other : server.connections) {
								auto other_num = players.find(&other);
								if (other_num != players.end() && other_num->second != player) {
									other.send_raw("m", 1);
									other.send_raw(&player, 1);
									other.send_raw(&dir, 1);
									other.send_raw(&target.x, sizeof(float));
									other.send_raw(&target.y, sizeof(float));
								}
							}

							std::cout << "Updated dir: " << ((int)dir) << ", for player: " << ((int)player) << std::endl;
						}
					}
				} else {
					std::cerr << "Unknown message header: " << c->recv_buffer[0] << std::endl;
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
				}
			}
		}, 0.01);
		
		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		if (start_count <= 0) {
			float elapsed = std::chrono::duration< float >(current_time - previous_time).count();

			static float total = 0.f;
			static float sync_time = 0.f;

			total += elapsed;
			sync_time += elapsed;
			while(total > 1.f/60.f) {
				total -= 1.f/60.f;
				if (state.update(1.f/60.f, true)) {
					// New apple pos
					state.apple_pos = vec2(((int)(rnd() % (2 * Game::BOARD_WIDTH + 1))) - Game::BOARD_WIDTH,
											((int)(rnd() % (2 * Game::BOARD_HEIGHT + 1))) - Game::BOARD_HEIGHT);
					for (Connection &conn : server.connections) {
						conn.send_raw("a", 1);
						conn.send_raw(&state.apple_pos.x, sizeof(float));
						conn.send_raw(&state.apple_pos.y, sizeof(float));
					}
				}

				// Check for dead sneks
				int dead = 0;
				for (int i=0; i<state.snakes.size(); i++) {
					Snake *snake = state.snakes[i];
					if (snake->dead) {
						dead++;
						for (auto pair : players) {
							if (pair.second == i) {
								// Send death
								pair.first->send_raw("d", 1);
								players.erase(pair.first);
								break;
							}
						}
					}
				}

				if (dead == 1 && !players.empty()) {

					std::cout << "DEAD: " << dead << std::endl;

					// Send victory
					players.begin()->first->send_raw("v", 1);
					initialize_game();
					break;
				}
			}

			if (sync_time >= .2f && start_count <= 0) {
				// Sync states
				state.send_sync(server.connections);
				sync_time -= 0.2f;
				std::cout << state.snakes[0]->head->front.x << ", " << state.snakes[0]->head->front.y << std::endl;
			}
		}

		previous_time = current_time;
	}
}
