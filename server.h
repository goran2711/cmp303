#pragma once
#include "network.h"
#include <atomic>
#include <condition_variable>

namespace Network
{
	namespace Server
	{
		bool StartServer(const sf::IpAddress& address, Port port);
		void ServerTask(const sf::IpAddress& address, Port port);
		void CloseServer();
	}
}