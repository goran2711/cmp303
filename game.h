#pragma once
#include <SFML/System.hpp>

// RECODE: no-no
#define PADDLE_SPEED	200.0f

struct Entity;

namespace Game
{
	sf::Vector2f MoveEntity(Entity& entity, const sf::Vector2f& velocity, float dt);
}