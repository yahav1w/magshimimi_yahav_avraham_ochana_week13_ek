#include "Server.h"
#include "Helper.h"
#include "WSAInitializer.h"
#include <exception>
#include <iostream>
#include <string>
#include <fstream>
#include <thread>
#include <condition_variable>
#include <sstream>
#include <iomanip>
std::condition_variable cv;
//the length of &MAGSH_MESSAGE&&Author&
#define MSG_LEN_START 23
Server::Server()
{

	// this server use TCP. that why SOCK_STREAM & IPPROTO_TCP
	// if the server use UDP we will use: SOCK_DGRAM & IPPROTO_UDP
	_serverSocket = socket(AF_INET,  SOCK_STREAM,  IPPROTO_TCP); 

	if (_serverSocket == INVALID_SOCKET)
		throw std::exception(__FUNCTION__ " - socket");
}

Server::~Server()
{
	try
	{
		// the only use of the destructor should be for freeing 
		// resources that was allocated in the constructor
		closesocket(_serverSocket);
	}
	catch (...) {}
}

void Server::serve(int port)
{
	
	struct sockaddr_in sa = { 0 };
	
	sa.sin_port = htons(port); // port that server will listen for
	sa.sin_family = AF_INET;   // must be AF_INET
	sa.sin_addr.s_addr = INADDR_ANY;    // when there are few ip's for the machine. We will use always "INADDR_ANY"

	// Connects between the socket and the configuration (port and etc..)
	if (bind(_serverSocket, (struct sockaddr*)&sa, sizeof(sa)) == SOCKET_ERROR)
		throw std::exception(__FUNCTION__ " - bind");
	
	// Start listening for incoming requests of clients
	if (listen(_serverSocket, SOMAXCONN) == SOCKET_ERROR)
		throw std::exception(__FUNCTION__ " - listen");
	std::cout << "Listening on port " << port << std::endl;

	while (true)
	{
		// the main thread is only accepting clients 
		// and add then to the list of handlers
		std::cout << "Waiting for client connection request" << std::endl;
		acceptClient();
	}
}


void Server::acceptClient()
{

	// this accepts the client and create a specific socket from server to this client
	// the process will not continue until a client connects to the server
	SOCKET client_socket = accept(_serverSocket, NULL, NULL);
	if (client_socket == INVALID_SOCKET)
		throw std::exception(__FUNCTION__);

	std::cout << "Client accepted. Server and client can speak" << std::endl;
	// the thread the use the function that handle the conversation with the client
	std::thread UserN(&Server::clientHandler,this,client_socket);
	UserN.detach();
}


void Server::clientHandler(SOCKET clientSocket)
{
	try
	{
		int messageType = Helper::getMessageTypeCode(clientSocket);
		if (messageType == MT_CLIENT_LOG_IN)
		{
			int usernameLength = Helper::getIntPartFromSocket(clientSocket, 2);
			std::string username = Helper::getStringPartFromSocket(clientSocket, usernameLength);
			{
				std::lock_guard<std::mutex> lock(clientsMutex);
				clients[clientSocket] = username;
			}
			sendUserListUpdate();
		}

		while (true)
		{
			int messageType = Helper::getMessageTypeCode(clientSocket);
			if (messageType == MT_CLIENT_UPDATE) 
			{
				handleClientMessage(clientSocket);
			}
			else if (messageType == MT_CLIENT_EXIT) 
			{
				removeClient(clientSocket);
				break;
			}
		}
	}
	catch (...)
	{
		removeClient(clientSocket);
	}


}

void Server::handleClientMessage(SOCKET clientSocket) 
{
	int userLegnth = Helper::getIntPartFromSocket(clientSocket, 2);
	std::string targetUser = Helper::getStringPartFromSocket(clientSocket, userLegnth);
	int messageLength = Helper::getIntPartFromSocket(clientSocket, 5);
	if (messageLength == 0)
	{
		sendUserListUpdate();
	}
	else
	{
		std::string message = Helper::getStringPartFromSocket(clientSocket, messageLength);
		std::cout << 1111 << targetUser << "\n";
		std::cout << userLegnth << "   " << messageLength << "\n";
		saveChatHistory(clients[clientSocket], targetUser, message);
		sendChat(clientSocket, clients[clientSocket], targetUser, message);
	}
	
}

void Server::saveChatHistory(const std::string& sender, const std::string& receiver, const std::string& message)
{
	std::string chatFile = sender < receiver ? sender + "&" + receiver + ".txt" : receiver + "&" + sender + ".txt";
	std::ofstream file(chatFile, std::ios::app);
	if (file.is_open()) 
	{
		file << "&MAGSH_MESSAGE&&Author&" << sender << "&DATA&" << message << "\n";
		file.close();
	}
}
void Server::sendChat(SOCKET client,const std::string& sender, const std::string& receiver, const std::string& message)
{
	int size = MSG_LEN_START + sender.size() + 6 + message.size();
	std::string allUsers;
	std::string msg = "101" + fiveNumLen(size) + "&MAGSH_MESSAGE&&Author&" + sender;
	msg += "&DATA&" + message + twoNumLen(sender.size()) + sender;
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		auto it = clients.begin();
		while (it != clients.end())
		{
			allUsers += it->second;
			++it;
			if (it != clients.end())
			{
				allUsers += "&";//the last user should not add a & so it doesnt add a user that does not exist
			}
		}
	}
	msg += fiveNumLen(allUsers.size()) + allUsers;
	std::lock_guard<std::mutex> lock(clientsMutex);
	std::cout <<"\n"<< msg<<"\n";
	Helper::sendData(client, msg);
}

void Server::sendUserListUpdate() 
{
	std::lock_guard<std::mutex> lock(clientsMutex);
	if (clients.empty()) return; // Prevent crash when no clients

	std::string allUsers;
	auto it = clients.begin();
	while (it != clients.end()) 
	{
		allUsers += it->second;
		++it;
		if (it != clients.end())
		{
			allUsers += "&";//the last user should not add a & so it doesnt add a user that does not exist
		}
	}

	for (auto it = clients.begin(); it != clients.end(); ++it) {
		Helper::send_update_message_to_client(it->first, "", "", allUsers);
	}
	
}
void Server::removeClient(SOCKET clientSocket) {
	{
		std::lock_guard<std::mutex> lock(clientsMutex);
		clients.erase(clientSocket);
	}
	closesocket(clientSocket);
	sendUserListUpdate();
}
//make and integer into five size string for example 5 -> 00005 and 15 -> 00015 or 1765 -> 01765
std::string Server::fiveNumLen(int num) 
{
	std::ostringstream oss;
	oss << std::setw(5) << std::setfill('0') << num;
	return oss.str();
}
//make and integer into two size string for example 5 -> 05 and 15 -> 15
std::string Server::twoNumLen(int num)
{
	std::ostringstream oss;
	oss << std::setw(2) << std::setfill('0') << num;
	return oss.str();
}
//find a socket by it username
SOCKET Server::findClientSocketByUsername(const std::string& username)
{
	std::lock_guard<std::mutex> lock(clientsMutex); // Ensure thread safety
	for (const auto& pair : clients)
	{
		std::cout << pair.second;
		std::cout << username;
		if (pair.second == username)
		{
			std::cout << "\nworked";
			return pair.first; // Return the corresponding socket
		}
	}
	return INVALID_SOCKET; 
}