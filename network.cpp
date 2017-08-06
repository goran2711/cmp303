#include "network.h"

namespace Network
{
	sf::Packet InitPacket(PacketType type)
	{
		sf::Packet packet;
		packet << sf::Uint8(type);
		return packet;
	}

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
}