#pragma comment(lib, "Ws2_32.lib")

#include "hyacinth-client.h"

int HyacinthNetworkClient::setup(std::string serveraddr) {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSA startup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    iResult = getaddrinfo(serveraddr.c_str(), DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    connectSocket = INVALID_SOCKET;
    connectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (connectSocket == INVALID_SOCKET) {
        std::cout << "problem with socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    const char* msg = "hello server";
    sendto(connectSocket, msg, strlen(msg), 0, result->ai_addr, (int)result->ai_addrlen);

    serverAddress = result->ai_addr;
    serverAddressLen = (int)result->ai_addrlen;
    connected = true;

    return 0;
}

void HyacinthNetworkClient::sendPositionString(Transform& t) {
    if (!connected) return;
    std::string s = glm::to_string(t.position);
    const char* msg = s.c_str();
    sendto(connectSocket, msg, strlen(msg), 0, serverAddress, serverAddressLen);
}