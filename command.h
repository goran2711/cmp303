#pragma once
#include <cstdint>
#include <SFML/System.hpp>
	
namespace sf
{
	class Packet;
}

struct Command
{
	sf::Uint32 id;
	// Keyboard state
	enum Direction
	{
		IDLE, LEFT, RIGHT
	} direction;
	sf::Uint64 dt;
};

sf::Packet& operator<<(sf::Packet& p, const Command& cmd);
sf::Packet& operator >> (sf::Packet& p, Command& cmd);
