/*
	Project is x64
*/
// Project is x64

#include <thread>
#include "server.h"
#include "client.h"
#include "debug.h"
using namespace Network;

#define SERVERIP	"127.0.0.1"
#define SERVERPORT	5555

void WaitTillServerStops()
{
	std::mutex dummyMutex;
	std::unique_lock<std::mutex> dummyLock(dummyMutex);

	Server::gCondServerClosed.wait(dummyLock, [&] { return !Server::gIsServerRunning.load(); });
}

int main()
{
	debug << "y: host game\nn: join game\nd: dedicated server\n";
	char input{};
	std::cin >> input;

	input = tolower(input);

	bool isHost = (input == 'y' || input == 'd');
	bool isDedicated = input == 'd';

	if (isHost)
		Server::StartServer({ SERVERIP }, SERVERPORT);

	if (isDedicated)
		WaitTillServerStops();
	else
		Client::StartClient({ SERVERIP }, SERVERPORT);

	if (isHost)
		Server::CloseServer();

	return 0;
}