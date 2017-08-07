#pragma once
#include "network.h"

namespace Network
{
	namespace Client
	{
		bool StartClient(const sf::IpAddress& address, Port port, bool isHost);
	}
}
