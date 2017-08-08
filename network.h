#pragma once
#include <SFML/Network.hpp>
#include <memory>

// Network.h: Contains code that is shared between Client and Server

#define DEF_SERVER_RECV(type)		void Receive_ ## type ## (ConnectionPtr connection, sf::Packet& p)
#define DEF_SERVER_SEND(type)		void Send_ ## type ## (ConnectionPtr connection)

#define DEF_CLIENT_RECV(type)		bool Receive_ ## type ## (sf::Packet& p)
#define DEF_CLIENT_SEND(type)		void Send_ ## type ## ()

#define DEF_SEND_PARAM(type)		void Send_ ## type

#define RECV(type)					Receive_ ## type
#define SEND(type)					Send_ ## type

namespace Network
{
	using Status = sf::Socket::Status;
	using Port = unsigned short;

	enum ConnectionStatus
	{
		STATUS_NONE,
		STATUS_JOINING,
		STATUS_PLAYING,
		STATUS_SPECTATING,
	};

	struct Connection
	{
		bool Connect(const sf::IpAddress& ip, Port port);
		void Disconnect();

		void Send(sf::Packet& p);
		bool Receive(sf::Packet& p);

		void SetBlocking(bool val);

		uint8_t pid;
		bool active = false;
		ConnectionStatus status = STATUS_NONE;
		sf::TcpSocket socket;
	};

	using ConnectionPtr = std::shared_ptr<Connection>;

	enum PacketType
	{
		PACKET_CLIENT_JOIN,			// Request from the client to the server to join
		PACKET_SERVER_WELCOME,		// Packet letting the client know they can join as a player
		PACKET_SERVER_SPECTATOR,	// Packet letting the client know they can join as a spectator
		PACKET_SERVER_FULL,			// Packet letting the client know it is full
		PACKET_CLIENT_CMD,			// Packet containing a movement command from a client
		PACKET_SERVER_UPDATE,		// Packet from the server containing the current state of the game
		PACKET_CLIENT_SHOOT,		// Request from the client to spawn a bullet
		PACKET_END,
	};

	sf::Packet InitPacket(PacketType type);
}

// Overloads
sf::Packet& operator<<(sf::Packet& p, const sf::Vector2f& v);
sf::Packet& operator >> (sf::Packet& p, sf::Vector2f& v);

template<typename T>
sf::Packet& operator<<(sf::Packet& p, const std::vector<T>& v)
{
	p << v.size();
	for (const auto& i : v)
		p << i;

	return p;
}

template<typename T>
sf::Packet& operator >> (sf::Packet& p, std::vector<T>& v)
{
	size_t size;
	p >> size;
	v.resize(size);

	for (auto& i : v)
		p >> i;

	return p;
}
