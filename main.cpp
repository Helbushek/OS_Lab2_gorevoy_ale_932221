#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

int serverSocket;
int clientSocket;
constexpr int PortNumber = 8009;

bool isFinished = false;
bool isConnected = false;

void Receive()
{
    int bufferSize = 1024;
    char buffer[1024];

    while (!isFinished)
    {
        int bytesReceived = recv(clientSocket, buffer, bufferSize, 0);

        if (bytesReceived == -1)
        {
            std::cerr << "Recv error" << std::endl;
            std::cout << strerror(errno) << std::endl;
        }
        else if (bytesReceived == 0)
        {
            std::cout << "Connection closed by peer" << std::endl;
            isConnected = false;
            return;
        }
        else 
        {
            std::cout << "Bytes received: " << std::to_string(bytesReceived) << " bytes: ";
            std::cout.write(buffer, bytesReceived) << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void Server()
{
    while (!isFinished)
    {
        sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressSize);
        if (clientSocket == -1) 
        {
            std::cout << "Error during connection acception" << std::endl;
        }
        std::cout << "Accepted connection" << std::endl;
        
        Receive();
    }
}

void InitializeServer()
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1)
    {
        std::cout << "Couldn`t open server`s socket" << std::endl; 
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; 
    serverAddress.sin_port = htons(PortNumber);

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) 
    {
        std::cout << "Couldn`t bind socket to address" << std::endl;
        std::cout << strerror(errno) << std::endl;
        return;
    }

    if (listen(serverSocket, SOMAXCONN) == -1) 
    {
        std::cout << "Error during socket listen" << std::endl;
    }

    Server();
}

int main()
{
    std::thread serverThread(InitializeServer);
    serverThread.join();
}