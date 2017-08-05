#include "network.h"

namespace Network
{
	ClientSocket::ClientSocket(Socket&& socket)
		:	socket(std::move(socket))
	{
	}

	Status ClientSocket::send(sf::Packet p)
	{
		// TODO: Checks here, doing _something_
		// in case of a disconnection (mark ClientSocket
		// as canSend = false or something)
		return socket->send(p);
	}

	Status ClientSocket::receive(sf::Packet& p)
	{
		return socket->receive(p);
	}
	Socket GetNewSocket()
	{
		return std::make_unique<sf::TcpSocket>();
	}

	sf::Packet InitPacket(PacketType type)
	{
		sf::Packet packet;
		packet << sf::Uint8(type);
		return packet;
	}
}