#pragma once
#include "network.h"

#define MAX_CLIENTS		2

namespace Network
{
	namespace Server
	{
		bool StartServer(const sf::IpAddress& address, Port port);
		void CloseServer();
	}
}