/*
	Project is x64
*/

#include <thread>
#include "server.h"
#include "client.h"
#include "debug.h"
using namespace Network;

constexpr char DEFAULT_IP[] = "127.0.0.1";
constexpr Port DEFAULT_PORT = 11223;

int main(int argc, const char* argv[])
{
	std::string serverip;
	Port serverport;

	if (argc >= 2)
	{
			serverip = argv[1];
			serverport = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;
			debug << "Using custom address: "; 
	}
	else
	{
			serverip = DEFAULT_IP;
			serverport = DEFAULT_PORT;
			debug << "Using default address: ";
	}
	debug << serverip << ':' << serverport << std::endl;

	debug << "Y: Host new game\n" <<
			"N: Join game in progress\n" << 
			"D: Run as dedicated server" << std::endl;

	char input{};
	std::cin >> input;

	input = tolower(input);

	bool isHost = (input == 'y');
	bool isDedicated = (input == 'd');

	// Start server in separate thread
	if (isHost)
		Server::StartServer({ serverip }, serverport);

	// Start server in main thread
	if (isDedicated)
		Server::ServerTask({ serverip }, serverport);
	// Start client
	else
		Client::StartClient({ serverip }, serverport);

	// Join server thread	if (isHost)
		Server::CloseServer();

	return 0;
}
