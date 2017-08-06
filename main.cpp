/*
	Project is x64
*/
// Project is x64

#include <iostream>
#include <thread>
#include "server.h"
#include "client.h"

#define SERVERIP	"127.0.0.1"
#define SERVERPORT	5555

int main()
{
	using namespace Network;
	std::mutex dummyMutex;
	std::unique_lock<std::mutex> dummyLock(dummyMutex);

	std::cout << "y: host game\nn: join game\nd: dedicated server\n";
	char input{};
	std::cin >> input;

	if (input == 'y' || input == 'd')
		Server::StartServer({ SERVERIP }, SERVERPORT);

	if (input != 'd')
		Client::StartClient({ SERVERIP }, SERVERPORT);
	else
		Server::_condServerClosed.wait(dummyLock, [&] { return !Server::_isServerRunning.load(); });

	if (input == 'y' || input == 'd')
		Server::CloseServer();

	return 0;
}