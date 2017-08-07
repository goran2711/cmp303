#pragma once
#include "network.h"
#include <atomic>
#include <condition_variable>

namespace Network
{
	namespace Server
	{
		extern std::atomic<bool> gIsServerRunning;
		extern std::condition_variable gCondServerClosed;

		bool StartServer(const sf::IpAddress& address, Port port);
		void ServerTask(const sf::IpAddress& address, Port port);
		void CloseServer();
	}
}