#pragma once
#include <SFML/Network.hpp>
#include <memory>

#define WAIT_TIME_MS	50

#define DEF_SERVER_RECV(type)		void Receive_ ## type ## (ClientSocket& cs, sf::Packet& p)
#define DEF_SERVER_SEND(type)		void Send_ ## type ## (ClientSocket& cs)

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
	using ClientID = uint8_t;
	using UID = uint32_t;
	using Port = unsigned short;
	using Socket = std::unique_ptr<sf::TcpSocket>;

	struct ClientInfo
	{
		ClientID id;
		Entity* entity = nullptr;
	};

	struct ClientSocket
	{
		ClientSocket() = default;
		ClientSocket(Socket&& socket);

		Status send(sf::Packet p);
		Status receive(sf::Packet& p);

		ClientInfo info;
		Socket socket = nullptr;
		bool hasQuit = false;

		int lastSeqNum = 0;
	};

	enum PacketType
	{
		PACKET_CLIENT_JOIN,
		PACKET_SERVER_WELCOME,
		PACKET_SERVER_JOIN,
		PACKET_SERVER_FULL,
		PACKET_CLIENT_QUIT,
		PACKET_SERVER_QUIT,
		PACKET_SERVER_PLAYER_INFO,
		PACKET_CLIENT_MOVE,
		PACKET_END,
	};

	enum NetworkReceiveError
	{
		RECEIVE_ERROR_OK,
		RECEIVE_ERROR_LOST_CONNECTION,
		RECEIVE_ERROR_OTHER
	};

	Socket GetNewSocket();
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
}