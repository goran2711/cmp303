#include "network.h"

namespace Network
{
	sf::Packet InitPacket(PacketType type)
	{
		sf::Packet packet;
		packet << sf::Uint8(type);
		return packet;
	}
}