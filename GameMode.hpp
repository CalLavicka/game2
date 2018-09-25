#pragma once

#include "Mode.hpp"
#include "Scene.hpp"

#include "MeshBuffer.hpp"
#include "GL.hpp"
#include "Connection.hpp"
#include "Game.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode : public Mode {
	GameMode(Client &client);
	virtual ~GameMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//------- game state -------
	Game state;
	bool initiated = false;
	bool started = false;
	bool lose = false;
	bool win = false;

	void load_objects();

	// Client side scene
	Scene scene;
	// Own snake
	Snake * player_snake;
	Scene::Object * apple_object;

	//------ networking ------
	Client &client; //client object; manages connection to server.
};
