#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define DEFAULT_PORT "6767"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include "hyacinth_physics.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Physics.lib")

#define DEFAULT_LEN 512

int main()
{
    hyacinthPhysicsTest();

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
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }

    // TODO: research Dual-Stack Sockets
    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

    if (listenSocket == INVALID_SOCKET) {
        std::cout << "problem with socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(listenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "bind failed: " << iResult << std::endl;
        freeaddrinfo(result);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    char ipStr[INET_ADDRSTRLEN];
    struct sockaddr_in* ipv4 = (struct sockaddr_in*)result->ai_addr;
    void* addr = &(ipv4->sin_addr);
    InetNtopA(AF_INET, addr, ipStr, sizeof(ipStr));

    std::cout << "listening on: " << std::string(ipStr) << std::endl;

    freeaddrinfo(result);

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

    char recvBuff[DEFAULT_LEN];
    sockaddr_in clientAddr;
    while (true) {
        int clientAddrSize = sizeof(clientAddr);

        int bytesReceived = recvfrom(listenSocket, recvBuff, sizeof(recvBuff) - 1, 0, (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            std::cout << "recvfrom failed: " << WSAGetLastError() << std::endl;
            break;
        }

        recvBuff[bytesReceived] = '\0';

        char clientIP[INET_ADDRSTRLEN];
        InetNtopA(AF_INET, &(clientAddr.sin_addr), clientIP, sizeof(clientIP));
        std::cout << "Received " << bytesReceived << " bytes from " << clientIP << ": " << recvBuff << std::endl;
    }

    closesocket(listenSocket);

    WSACleanup();
    return 0;
}