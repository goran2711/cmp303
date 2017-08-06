#include "command.h"
#include <SFML/Network.hpp>

sf::Packet& operator<<(sf::Packet& p, const Command& cmd)
{
	int direction = cmd.direction;
	p << cmd.id << cmd.dt << direction;
	return p;
}

sf::Packet& operator >> (sf::Packet& p, Command& cmd)
{
	int direction;
	p >> cmd.id >> cmd.dt >> direction;
	cmd.direction = (Command::Direction)direction;
	return p;
}
