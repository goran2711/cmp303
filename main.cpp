/*
	Project is x64
*/

#include <thread>
#include "server.h"
#include "client.h"
#include "debug.h"
using namespace Network;

constexpr char SERVERIP[] = "127.0.0.1";
constexpr Port SERVERPORT = 5555;

int main()
{
	debug << "y: host game\nn: join game\nd: dedicated server\n";
	char input{};
	std::cin >> input;

	input = tolower(input);

	bool isHost = (input == 'y');
	bool isDedicated = (input == 'd');

	// Start server in separate thread
	if (isHost)
		Server::StartServer({ SERVERIP }, SERVERPORT);

	// Start server in main thread
	if (isDedicated)
		Server::ServerTask({ SERVERIP }, SERVERPORT);
	// Start client
	else
		Client::StartClient({ SERVERIP }, SERVERPORT);

	// Join server thread
	if (isHost)
		Server::CloseServer();

	return 0;
}