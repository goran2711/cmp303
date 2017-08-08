#include "network.h"

namespace Network
{
	bool Connection::Connect(const sf::IpAddress & ip, Port port)
	{
		if (socket.connect(ip, port) != Status::Done)
			return false;

		active = true;
		return true;
	}

	void Connection::Disconnect()
	{
		socket.disconnect();
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

	void Connection::SetBlocking(bool val)
	{
		socket.setBlocking(val);
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

