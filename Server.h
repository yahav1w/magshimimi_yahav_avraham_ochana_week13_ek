#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include "mutex"
#include <map>

class Server
{
public:
	Server();
	~Server();
	void serve(int port);

private:

	void acceptClient();
	void clientHandler(SOCKET clientSocket);
	void handleClientMessage(SOCKET clientSocket);
	void saveChatHistory(const std::string& sender, const std::string& receiver, const std::string& message);
	void sendChat(SOCKET client, const std::string& sender, const std::string& receiver, const std::string& message);
	void sendUsersListUpdate();
	void sendUserList(SOCKET client);
	void removeClient(SOCKET clientSocket);

	std::string fiveNumLen(int num);

	std::string twoNumLen(int num);

	SOCKET findClientSocketByUsername(const std::string& username);

	bool isSocketDisconnected(SOCKET socket);

	std::mutex clientsMutex;
	std::map<SOCKET, std::string> clients;
	std::condition_variable cv;
	std::unique_lock<std::mutex> user;
	std::string userStr;
	SOCKET _serverSocket;
};

