#pragma once
#include "network.h"
#include <atomic>
#include <condition_variable>

#define MAX_CLIENTS		2

namespace Network
{
	namespace Server
	{
		extern std::atomic<bool> _isServerRunning;
		extern std::condition_variable _condServerClosed;

		bool StartServer(const sf::IpAddress& address, Port port);
		void CloseServer();
	}
}