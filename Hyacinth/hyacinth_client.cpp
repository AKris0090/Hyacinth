#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Common.lib")

#include "hyacinth_client.h"

void HyacinthNetworkClient::listenForServer(SOCKET udpReceiverSocket) {
    char recvBuff[DEFAULT_LEN];

    while (true) {
        int bytesReceived = recvfrom(udpReceiverSocket, recvBuff, sizeof(recvBuff) - 1, 0, NULL, NULL);

        if (bytesReceived <= 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                std::cout << "network error, not receiving packets from server (last 5 seconds)" << std::endl;
            }
            continue;
        }

        if (bytesReceived == SOCKET_ERROR) {
            std::cout << "recvfrom failed: " << WSAGetLastError() << std::endl;
            break;
        }

        recvBuff[bytesReceived] = '\0';
        if (std::string(recvBuff) == "pong") {
            continue;
        }

        ServerSnapshot sp = ServerSnapshot::fromString(std::string(recvBuff));

        netEntManager.packetBuffer.newPacket(sp);

        netEntManager.rB.pendingPacketsMutex.lock();
        netEntManager.rB.pendingPackets.push(sp);
        netEntManager.rB.pendingPacketsMutex.unlock();
    }

    closesocket(udpReceiverSocket);
}

int HyacinthNetworkClient::setup(std::string serveraddr, SWChainImageFormat swImageFormat, VkDescriptorSetLayout& uniformLayout) {
    netEntManager.self = new Entity();

    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "WSA startup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, hints;

    // create UDP receiver socket
    udpReceiverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpReceiverSocket == INVALID_SOCKET) {
        std::cout << "problem with udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    sockaddr_in recvAddr;
    ZeroMemory(&recvAddr, sizeof(recvAddr));
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_addr.s_addr = INADDR_ANY;
    recvAddr.sin_port = 0;

    iResult = bind(udpReceiverSocket, (sockaddr*)&recvAddr, sizeof(recvAddr));
    if (iResult != 0) {
        std::cout << "udp receiver socket binding failed: " << iResult << std::endl;
        return 1;
    }

    sockaddr_in boundAddr;
    int boundAddrLen = sizeof(boundAddr);
    if (getsockname(udpReceiverSocket, (sockaddr*)&boundAddr, &boundAddrLen) == SOCKET_ERROR) {
        std::cout << "getsockname failed: " << WSAGetLastError() << std::endl;
        closesocket(udpReceiverSocket);
        WSACleanup();
        return 1;
    }
    receiverPort = ntohs(boundAddr.sin_port); // ntohs called, so receiver must call htons() when decrypting
    std::cout << "receiver set up on port: " << receiverPort << std::endl;
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
    myRequest.port = static_cast<uint32_t>(receiverPort);
    std::string requestString = myRequest.toString();

    uint64_t timeStampPing = getNowMs();
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
    uint64_t timeStampPong = getNowMs();
    recvbuf[iResult] = '\0';

    // get the entity list from the server
    char entityBuff[DEFAULT_LEN];
    int entityBuffLen = DEFAULT_LEN;
    iResult = recv(connectSocket, entityBuff, entityBuffLen, 0);
    if (iResult <= 0) {
        std::cout << "could not receive entity list from server!" << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    entityBuff[iResult] = '\0';

    iResult = shutdown(connectSocket, SD_BOTH);
    if (iResult == SOCKET_ERROR) {
        std::cout << "secondary shutdown failed: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    uint64_t rtt = timeStampPong - timeStampPing;
    uint64_t halfRtt = rtt / 2;
    uint64_t ticksInFlight = halfRtt / (SERVER_TIMESTEP * 1000.f);

    std::cout << "RTT / 2: " << halfRtt << std::endl;

    ClientRequestConnectionPacket serverResponse;
    serverResponse.fromString(std::string(recvbuf));
    clientID = serverResponse.port;
    netEntManager.self->id = clientID;

    ServerSnapshot sp;
    sp = ServerSnapshot::fromString(std::string(entityBuff));
    netEntManager.imageFormat = swImageFormat;
    netEntManager.uniformSetLayout = &uniformLayout;
    netEntManager.setupFromServerPacket(sp, clientID);

    std::cout << "my id is: " << clientID << std::endl;

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    if (setsockopt(udpReceiverSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        std::cout << "socket timeout set error" << std::endl;
        return 1;
    }

    std::stringstream ss;
    ss << "tryping" << clientID;
    while (true) {
        sendto(udpReceiverSocket, ss.str().c_str(), ss.str().length(), 0, (sockaddr*)&serverAddress, serverAddressLen);
        std::cout << "SENT PING" << std::endl;

        int res = recvfrom(udpReceiverSocket, entityBuff, entityBuffLen, 0, NULL, NULL);
        if (res != 4) { // received should be "pong"
            continue;
        } 

        std::cout << "SERVER ACK, starting now..." << std::endl;
        break;
    }

    connected = true;

    std::thread serverListenThread(&HyacinthNetworkClient::listenForServer, this, udpReceiverSocket);
    serverListenThread.detach();

    return 0;
}

void HyacinthNetworkClient::updateServerTick(ClientUpdatePacket& p, bool mouseLocked) {
    std::string s = p.toString();
    const char* msg = s.c_str();
    sendto(udpReceiverSocket, msg, strlen(msg), 0, (sockaddr*)&serverAddress, serverAddressLen);
}

void HyacinthNetworkClient::shutdownNet() {
    // netEntManager.shutdown();
}