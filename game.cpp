#include "game.h"
#include "entity.h"

sf::Vector2f Game::MoveEntity(Entity& entity, const sf::Vector2f& velocity, float dt)
{
	auto moveDelta = velocity * dt;

	entity.position += moveDelta;

	return moveDelta;
}
