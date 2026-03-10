#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define DEFAULT_PORT "6767"
#define PACKET_PORT "6969"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <thread>

#include "hyacinth_network.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Common.lib")

uint32_t currentClientID = -1;
std::unordered_map<uint32_t, ServersideClient*> clients;

void handleNewClient(SOCKET* socket, ServersideClient* newClient) {
    int initialReq, serverAck;

    char recvbuf[DEFAULT_LEN];
    int recvbuflen = DEFAULT_LEN;
    initialReq = recv(*socket, recvbuf, recvbuflen, 0);

    if (initialReq > 0) {
        ClientRequestConnectionPacket p;
        p.fromString(std::string(recvbuf));
        newClient->clientAddr.sin_port = p.port;

        std::cout << "client added on port: " << newClient->clientAddr.sin_port << std::endl;

        ClientRequestConnectionPacket response;
        response.port = newClient->id;

        std::string msg = response.toString();
        serverAck = send(*socket, msg.c_str(), msg.length(), 0);
        if (serverAck == SOCKET_ERROR) {
            std::cout << "acknowledge failed to send?" << std::endl;
            closesocket(*socket);
            WSACleanup();
            clients.erase(newClient->id);
            return;
        }
        std::cout << "client requested connection, sent acknowledgement with id: " << newClient->id << std::endl;
        shutdown(*socket, SD_BOTH);
    }
    else {
        std::cout << "client initiation receive failure" << std::endl;
        clients.erase(newClient->id);
        closesocket(*socket);
        WSACleanup();
    }
}

void serverListenForClients(SOCKET* tcpSocket) {
    while (true) {
        if (listen(*tcpSocket, SOMAXCONN) == SOCKET_ERROR) {
            printf("Listen failed with error: %ld\n", WSAGetLastError());
            closesocket(*tcpSocket);
            WSACleanup();
            return;
        }

        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = INVALID_SOCKET;
        clientSocket = accept(*tcpSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            printf("accept failed: %d\n", WSAGetLastError());
            closesocket(*tcpSocket);
            WSACleanup();
            return;
        }

        currentClientID++;
        ServersideClient* newClient = new ServersideClient();
        newClient->id = currentClientID;
        newClient->clientAddr = clientAddr;
        clients[currentClientID] = newClient;

        std::thread newClientThread(handleNewClient, &clientSocket, newClient);
        newClientThread.detach();
    }
}

void serverListenForUDPPackets(SOCKET* udpSocket) {
    char recvBuff[DEFAULT_LEN];
    sockaddr_in clientAddr;

    while (true) {
        int clientAddrSize = sizeof(clientAddr);

        int bytesReceived = recvfrom(*udpSocket, recvBuff, sizeof(recvBuff) - 1, 0, (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            std::cout << "recvfrom failed: " << WSAGetLastError() << std::endl;
            break;
        }

        recvBuff[bytesReceived] = '\0';

        ClientUpdatePacket p = decomposePacket(recvBuff);
        p.print();
    }

    closesocket(*udpSocket);
}

int main()
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSA startup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    SOCKET tcpListenSocket = INVALID_SOCKET;
    tcpListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (tcpListenSocket == INVALID_SOCKET) {
        std::cout << "problem with tcp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(tcpListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "tcp bind failed: " << iResult << std::endl;
        freeaddrinfo(result);
        closesocket(tcpListenSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);
    std::thread tcpSocketThread(serverListenForClients, &tcpListenSocket);

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(NULL, PACKET_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    SOCKET udpListenSocket = INVALID_SOCKET;
    udpListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (udpListenSocket == INVALID_SOCKET) {
        std::cout << "problem with udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(udpListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "udp bind failed: " << iResult << std::endl;
        freeaddrinfo(result);
        closesocket(udpListenSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    // print host name for connecting
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    struct addrinfo* localResult = NULL;
    struct addrinfo localHints;
    ZeroMemory(&localHints, sizeof(localHints));
    localHints.ai_family = AF_INET;
    if (getaddrinfo(hostname, NULL, &localHints, &localResult) == 0) {
        char localIP[INET_ADDRSTRLEN];
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)localResult->ai_addr;
        InetNtopA(AF_INET, &(ipv4->sin_addr), localIP, sizeof(localIP));
        std::cout << "Connect from other devices using: " << localIP << ":" << DEFAULT_PORT << std::endl;
        freeaddrinfo(localResult);
    }

    // listen on the udp thread
    std::thread udpSocketThread(serverListenForUDPPackets, &udpListenSocket);

    tcpSocketThread.join();
    udpSocketThread.join();

    WSACleanup();
    return 0;
}