#pragma once
#include <cstdint>

namespace sf
{
	class Packet;
}

struct Command
{
	uint32_t id;
	// Keyboard state
	enum Direction
	{
		IDLE, LEFT, RIGHT
	} direction;
	uint64_t dt;
};

sf::Packet& operator<<(sf::Packet& p, const Command& cmd);
sf::Packet& operator >> (sf::Packet& p, Command& cmd);