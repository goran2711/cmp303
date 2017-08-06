#include "network.h"

namespace Network
{
	void Connection::Send(sf::Packet& p)
	{
		Status ret;

		do
		{
			ret = socket.send(p);
		} while (ret == Status::Partial /* || ret == Status::NotReady */);

		switch (ret)
		{
			case Status::Disconnected:
			case Status::Error:
				active = false;
		}
	}

	bool Connection::Receive(sf::Packet & p)
	{
		Status ret;

		do
		{
			ret = socket.receive(p);
		} while (ret == Status::Partial);

		switch (ret)
		{
			case Status::Error:
			case Status::Disconnected:
				active = false;
			case Status::NotReady:
				return false;
		}

		return true;
	}

	sf::Packet InitPacket(PacketType type)
	{
		sf::Packet packet;
		packet << sf::Uint8(type);
		return packet;
	}
}

sf::Packet& operator<<(sf::Packet& p, const sf::Vector2f& v)
{
	p << v.x << v.y;
	return p;
}

sf::Packet& operator>>(sf::Packet& p, sf::Vector2f& v)
{
	p >> v.x >> v.y;
	return p;
}

