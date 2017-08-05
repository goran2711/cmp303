#pragma once
#include <SFML/Network.hpp>
#include <memory>
#include "command.h"
#include "player.h"
#include "world.h"

#define WAIT_TIME_MS	50

#define DEF_SERVER_RECV(type)		void Receive_ ## type ## (ConnectionPtr connection, sf::Packet& p)
#define DEF_SERVER_SEND(type)		void Send_ ## type ## (ConnectionPtr connection)

#define DEF_CLIENT_RECV(type)		NetworkReceiveError Receive_ ## type ## (sf::Packet& p)
#define DEF_CLIENT_SEND(type)		void Send_ ## type ## ()

#define DEF_SEND_PARAM(type)		void Send_ ## type

#define RECV(type)					Receive_ ## type
#define SEND(type)					Send_ ## type

#define INVALID_CID					0xFFFF

struct Entity;

namespace Network
{
	using Status = sf::Socket::Status;
	using Port = unsigned short;
	using Socket = std::unique_ptr<sf::TcpSocket>;

	struct Connection
	{
		// TODO: Abstract the socket

		uint8_t pid;
		bool active = false;
		sf::TcpSocket socket;
	};

	using ConnectionPtr = std::shared_ptr<Connection>;

	enum PacketType
	{
		PACKET_CLIENT_JOIN,
		PACKET_SERVER_WELCOME,
		PACKET_SERVER_FULL,
		PACKET_CLIENT_CMD,
		PACKET_SERVER_UPDATE,
		PACKET_END,
	};

	enum NetworkReceiveError
	{
		RECEIVE_ERROR_OK,
		RECEIVE_ERROR_LOST_CONNECTION,
		RECEIVE_ERROR_OTHER
	};

	sf::Packet InitPacket(PacketType type);
}

// Overloads
namespace
{
	sf::Packet& operator<<(sf::Packet& p, const sf::Vector2f& v)
	{
		p << v.x << v.y;
		return p;
	}

	sf::Packet& operator >> (sf::Packet& p, sf::Vector2f& v)
	{
		p >> v.x >> v.y;
		return p;
	}

	// Move to cpp
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

	sf::Packet& operator<<(sf::Packet& p, const Player& player)
	{
		p << player.pid() << player.lastCommandID() << player.colour() << player.position();
		return p;
	}

	sf::Packet& operator >> (sf::Packet& p, Player& player)
	{
		uint8_t pid;
		int lastCommandID;
		uint32_t colour;
		sf::Vector2f position;
		p >> pid >> lastCommandID >> colour >> position;

		player.SetPID(pid);
		player.SetLastCommandID(lastCommandID);
		player.SetColour(colour);
		player.SetPosition(position);

		return p;
	}

	sf::Packet& operator<<(sf::Packet& p, const World& world)
	{
		p << world.players().size();
		for (const auto& player : world.players())
		{
			p << player;
		}

		return p;
	}

	sf::Packet& operator >> (sf::Packet& p, World& world)
	{
		size_t size;
		p >> size;
		world.players().resize(size);

		for (auto& player : world.players())
			p >> player;

		return p;
	}

	sf::Packet& operator<<(sf::Packet& p, const WorldSnapshot& snapshot)
	{
		p << snapshot.snapshot << snapshot.serverTime << snapshot.clientTime;
		return p;
	}

	sf::Packet& operator >> (sf::Packet& p, WorldSnapshot& snapshot)
	{
		p >> snapshot.snapshot >> snapshot.serverTime << snapshot.clientTime;
		return p;
	}
}