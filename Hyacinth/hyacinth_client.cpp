#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Common.lib")

#include "hyacinth_client.h"
#include "hyacinth_network.h"

int HyacinthNetworkClient::setup(std::string serveraddr) {
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSA startup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, hints;

    // create UDP receiver socket
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(NULL, MY_UDP_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    udpReceiverSocket = INVALID_SOCKET;
    udpReceiverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (udpReceiverSocket == INVALID_SOCKET) {
        std::cout << "problem with udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(udpReceiverSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "udp bind failed: " << iResult << std::endl;
        freeaddrinfo(result);
        closesocket(udpReceiverSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    // create server UDP sender socket
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;
    iResult = getaddrinfo(serveraddr.c_str(), SERVER_UDP_PORT, &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    serverUDPSocket = INVALID_SOCKET;
    serverUDPSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverUDPSocket == INVALID_SOCKET) {
        std::cout << "problem with server udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    memcpy(&serverAddress, result->ai_addr, result->ai_addrlen);
    serverAddressLen = (int)result->ai_addrlen;
    freeaddrinfo(result);

    // create TCP connection socket
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
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
    iResult = connect(connectSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(connectSocket);
        connectSocket = INVALID_SOCKET;
    }
    if(connectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server! Make sure it is running and listening!\n");
        WSACleanup();
        return 1;
    }
    std::cout << "connected to server!" << std::endl;
    freeaddrinfo(result);

    // send the server handshake packet after tcp connection established
    ClientRequestConnectionPacket myRequest;
    myRequest.port = static_cast<uint32_t>(std::stoi(MY_UDP_PORT));
    std::string requestString = myRequest.toString();

    int sendResult = send(connectSocket, requestString.c_str(), requestString.length(), 0);
    if (sendResult == SOCKET_ERROR) {
        std::cout << "request failed to send?" << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // get the server response to the join request. contains client id
    char recvbuf[DEFAULT_LEN];
    int recvbuflen = DEFAULT_LEN;
    iResult = recv(connectSocket, recvbuf, recvbuflen, 0);
    if (iResult <= 0) {
        std::cout << "could not receive response from server!" << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    recvbuf[iResult] = '\0';

    iResult = shutdown(connectSocket, SD_BOTH);
    if (iResult == SOCKET_ERROR) {
        std::cout << "secondary shutdown failed: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    ClientRequestConnectionPacket serverResponse;
    serverResponse.fromString(std::string(recvbuf));
    clientID = serverResponse.port;

    std::cout << "my id is: " << clientID << std::endl;

    connected = true;

    return 0;
}

void HyacinthNetworkClient::sendMovementString(Transform& t) {
    if (!connected) return;
    ClientUpdatePacket p;
    p.id = clientID;
    p.movementX = t.position.x;
    p.movementY = t.position.y;
    p.movementZ = t.position.z;
         
    std::string s = p.toString();
    const char* msg = s.c_str();
    sendto(serverUDPSocket, msg, strlen(msg), 0, (sockaddr*)&serverAddress, serverAddressLen);
}