#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "vertex_color_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>

#include <stdio.h>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("paddle-ball.pnc"));
});

Load< GLuint > meshes_for_vertex_color_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(vertex_color_program->program));
});

Scene::Transform *paddle_transform = nullptr;
Scene::Transform *ball_transform = nullptr;

Scene::Camera *camera = nullptr;

GameMode::GameMode(Client &client_) : client(client_) {
	{ // Create apple
		apple_object = scene.new_object(scene.new_transform());
		apple_object->program = vertex_color_program->program;
		apple_object->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		apple_object->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		apple_object->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		MeshBuffer::Mesh const &mesh = meshes->lookup("Apple.1");
		apple_object->vao = *meshes_for_vertex_color_program;
		apple_object->start = mesh.start;
		apple_object->count = mesh.count;

		apple_object->transform->rotation = angleAxis(3.1415f/2.f, vec3(1.f,0.f,0.f));
	}

	{ // Create frame
		Scene::Object *frame = scene.new_object(scene.new_transform());
		frame->program = vertex_color_program->program;
		frame->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		frame->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		frame->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		MeshBuffer::Mesh const &mesh = meshes->lookup("Frame");
		frame->vao = *meshes_for_vertex_color_program;
		frame->start = mesh.start;
		frame->count = mesh.count;
		
		frame->transform->scale = vec3(Game::MAX_X / 5.f, Game::MAX_Y / 4.f, 1.f);
	}
}

void GameMode::load_objects() {
	for (int i=0; i<state.snakes.size(); i++) {
		Snake *snake = state.snakes[i];
		Scene::SnakeObject * obj = scene.new_snake(snake);
		obj->program = vertex_color_program->program;
		obj->program_mvp_mat4  = vertex_color_program->object_to_clip_mat4;
		obj->program_mv_mat4x3 = vertex_color_program->object_to_light_mat4x3;
		obj->program_itmv_mat3 = vertex_color_program->normal_to_light_mat3;

		MeshBuffer::Mesh const &mesh = meshes->lookup(i == 0 ? "Green_Snake" : "Yellow_Snake");
		obj->vao = *meshes_for_vertex_color_program;
		obj->start = mesh.start;
		obj->count = mesh.count;


		MeshBuffer::Mesh const &joint = meshes->lookup("Green_Joint");
		obj->joint_start = joint.start;
		obj->joint_count = joint.count;
	}

	camera = scene.new_camera(scene.new_transform());
	camera->transform->position = vec3(0.f, 0.f, 10.f);
	camera->transform->rotation = angleAxis(0.f,vec3(0.f,0.f,1.f));
}

GameMode::~GameMode() {
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	auto send_turn = [&](char new_dir) {
		client.connection.send_raw("m", 1);
		client.connection.send_raw(&new_dir, 1);
		client.connection.send_raw(&player_snake->head->front.x, sizeof(float));
		client.connection.send_raw(&player_snake->head->front.y, sizeof(float));
	};

	if (evt.type == SDL_KEYDOWN) {
		int newdir;
		switch(evt.key.keysym.scancode) {
		case SDL_SCANCODE_D:
			newdir = (player_snake->dir + 3) % 4;//Snake::Direction::RIGHT;
			break;
		case SDL_SCANCODE_A:
			newdir = (player_snake->dir + 1) % 4;//Snake::Direction::LEFT;
			break;/*
		case SDL_SCANCODE_W:
			newdir = Snake::Direction::UP;
			break;
		case SDL_SCANCODE_S:
			newdir = Snake::Direction::DOWN;
			break;*/
		default:
			return false;
		}

		int dist = newdir - player_snake->dir;
		if(abs(dist) == 1 || abs(dist) == 3) {
			// Check to make sure not doubling back on own body
			if(player_snake->head->length > 0.4f && (player_snake->head->prev == nullptr ||
				player_snake->head->prev->dir == newdir || player_snake->head->length > 0.9f)) {
				send_turn(newdir);
				player_snake->change_dir(newdir);
			}
		}
		return true;
	}

	return false;
}

#define PI 3.14159265f

void GameMode::update(float elapsed) {

	if (started) {
		state.update(elapsed, false);
	}

	

	if (initiated){ 
		apple_object->transform->position = vec3(state.apple_pos, 0.f);
		
		// Update camera
		static float camera_yaw = 0.0f;
		float target_yaw = player_snake->camera_angle();
		float dist = target_yaw - camera_yaw;
		if (dist < -PI) {
			camera_yaw -= 2*PI;
			dist += 2 * PI;
		} else if (dist > PI) {
			camera_yaw += 2 * PI;
			dist -= 2 * PI;
		}

		#define CAMERA_SPEED 0.05f
		if (dist > CAMERA_SPEED) {
			camera_yaw += CAMERA_SPEED;
		} else if (dist < -CAMERA_SPEED) {
			camera_yaw -= CAMERA_SPEED;
		}

		camera->transform->position = vec3(player_snake->head->front - vec2(-sin(camera_yaw), cos(camera_yaw))*4.f, 8.f);
		camera->transform->rotation = angleAxis(camera_yaw, vec3(0.f, 0.f, 1.f)) * angleAxis(0.6f, vec3(1.f, 0.f,0.f));
	}

	client.poll([&](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			//probably won't get this.
		} else if (event == Connection::OnClose) {
			std::cerr << "Lost connection to server." << std::endl;
		} else { assert(event == Connection::OnRecv);
			if (!initiated) {
				if (c->recv_buffer.size() >= 3 + 2 * sizeof(float)) {
					assert(c->recv_buffer[0] == 'p');
					char num_players = c->recv_buffer[1];
					char my_player = c->recv_buffer[2];

					state.new_game((int)num_players);
					player_snake = state.snakes[my_player];
					load_objects();

					memcpy(&state.apple_pos.x, c->recv_buffer.data() + 3, sizeof(float));
					memcpy(&state.apple_pos.y, c->recv_buffer.data() + 3 + sizeof(float), sizeof(float));

					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 3 + 2 * sizeof(float));
					std::cout << "Initiated" << std::endl;
					initiated = true;
					c->send_raw("h", 1); //send a 'hello' to the server, to signal ready for game
				}
			} else {
				while(c->recv_buffer.size() > 0) {
					if (c->recv_buffer[0] == 's') {
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
						std::cout << "Started" << std::endl;
						started = true;
					} else if (c->recv_buffer[0] == 'm') {
						if (c->recv_buffer.size() < 3 + sizeof(float) + sizeof(float))
							return;
						char moved_player = c->recv_buffer[1];
						char new_dir = c->recv_buffer[2];
						vec2 target;
						memcpy(&target.x, c->recv_buffer.data() + 3, sizeof(float));
						memcpy(&target.y, c->recv_buffer.data() + 3 + sizeof(float), sizeof(float));
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 3 +
																	sizeof(float) + sizeof(float));

						state.snakes[moved_player]->revert_and_change(target, new_dir);


						std::cout << "Received move from server" << std::endl;
					} else if (c->recv_buffer[0] == 'y') {
						state.recv_sync(c);
					} else if (c->recv_buffer[0] == 'a') {
						if (c->recv_buffer.size() < 1 + sizeof(float) + sizeof(float))
							return;
						memcpy(&state.apple_pos.x, c->recv_buffer.data() + 1, sizeof(float));
						memcpy(&state.apple_pos.y, c->recv_buffer.data() + 1 + sizeof(float), sizeof(float));
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1 +
																	sizeof(float) + sizeof(float));

						std::cout << "Received apple pos: " << state.apple_pos.x << ", " << state.apple_pos.y << std::endl;
					} else if (c->recv_buffer[0] == 'd') {
						started = false;
						lose = true;
						return;
					} else if (c->recv_buffer[0] == 'v') {
						started = false;
						win = true;
						return;
					} else {
						std::cerr << "Unknown message header: " << c->recv_buffer[0] << std::endl;
						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 1);
					}
				}
			}
		}
	});

	//copy game state to scene positions:
	/*ball_transform->position.x = state.ball.x;
	ball_transform->position.y = state.ball.y;

	paddle_transform->position.x = state.paddle.x;
	paddle_transform->position.y = state.paddle.y;*/
}

void GameMode::draw(glm::uvec2 const &drawable_size) {
	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.25f, 0.1f, 0.45f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (win || lose) {
		glDisable(GL_DEPTH_TEST);
		std::string message;
		if (win) {
			message = "YOU WIN";
		} else {
			message = "YOU LOSE";
		}
		float height = 0.1f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,0.f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,0.02f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);

		return;
	}

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(vertex_color_program->program);

	glUniform3fv(vertex_color_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.81f, 0.81f, 0.76f)));
	glUniform3fv(vertex_color_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.2f, 0.2f, 1.0f))));
	glUniform3fv(vertex_color_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(vertex_color_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f)));

	scene.draw(camera);

	if(!started) {
		glDisable(GL_DEPTH_TEST);
		std::string message = "WAITING ON PLAYERS";

		float height = 0.1f;
		float width = text_width(message, height);
		draw_text(message, glm::vec2(-0.5f * width,-0.5f), height, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
		draw_text(message, glm::vec2(-0.5f * width,-0.48f), height, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		glUseProgram(0);
	}

	GL_ERRORS();
}
