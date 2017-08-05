#pragma once
#include <SFML/System.hpp>
#include <SFML/Graphics.hpp>
#include <vector>
#include "network.h"

#define INTERPOLATE_TIME	0.25f // same as server update freq

struct Entity
{
	uint8_t uid;
	bool isPlayer = false;
	
	sf::Vector2f position;
};

struct VisualEntity : Entity
{
	void AddSample(const sf::Vector2f& v);
	void Interpolate(float dt);
	void Draw(sf::RenderWindow& window);

	Network::ClientID id = INVALID_CID;

	// For interpolation
	float interpolationProgress;
	sf::Vector2f targetPosition;
	std::vector<sf::Vector2f> serverPositions;

	sf::RectangleShape sprite;
};