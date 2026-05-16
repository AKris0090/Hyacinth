#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Common.lib")

#include "hyacinth_client.h"

// assumes that socket s has a timeout set
inline std::string loopTillResponse(SOCKET* s, const char* str, int len, sockaddr* socketAddr, int addrLen) {
    char resBuffer[DEFAULT_LEN];
    int resBufferLen = DEFAULT_LEN;

    int res = 0;
    while (res <= 0) {
        int altRes = sendto(*s, str, len, 0, socketAddr, addrLen);
        if (altRes < 0) {
            throw std::runtime_error("[NETWORK] could not send, for some reason");
        }

        res = recvfrom(*s, resBuffer, resBufferLen, 0, NULL, NULL);
        if (res <= 0) {
            std::cout << "[NETWORK] error receiving info from server, trying again..." << std::endl;
        }
    }
    resBuffer[res] = '\0';
    return std::string(resBuffer);
}

void HyacinthNetworkClient::listenForServer(SOCKET twoWayUDPSocket) {
    char recvBuff[DEFAULT_LEN];

    while (true) {
        int bytesReceived = recvfrom(twoWayUDPSocket, recvBuff, sizeof(recvBuff) - 1, 0, NULL, NULL);

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
        else if (std::string(recvBuff).substr(0, 4) == "snap") {
            continue;
        }

        ServerSnapshot sp = ServerSnapshot::fromString(std::string(recvBuff));
        serverAck = sp.serverTickNum;
        netEntManager.packetBuffer.newPacket(sp);

        netEntManager.rB.pendingPacketsMutex.lock();
        netEntManager.rB.pendingPackets.push(sp);
        netEntManager.rB.pendingPacketsMutex.unlock();
    }

    closesocket(twoWayUDPSocket);
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
    twoWayUDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (twoWayUDPSocket == INVALID_SOCKET) {
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

    iResult = bind(twoWayUDPSocket, (sockaddr*)&recvAddr, sizeof(recvAddr));
    if (iResult != 0) {
        std::cout << "udp receiver socket binding failed: " << iResult << std::endl;
        return 1;
    }

    sockaddr_in boundAddr;
    int boundAddrLen = sizeof(boundAddr);
    if (getsockname(twoWayUDPSocket, (sockaddr*)&boundAddr, &boundAddrLen) == SOCKET_ERROR) {
        std::cout << "getsockname failed: " << WSAGetLastError() << std::endl;
        closesocket(twoWayUDPSocket);
        WSACleanup();
        return 1;
    }
    receiverPort = ntohs(boundAddr.sin_port); // ntohs called, so receiver must call htons() when decrypting
    std::cout << "receiver set up on port: " << receiverPort << std::endl;
    freeaddrinfo(result);

    // set socket timeout for 1 second
    DWORD timeout = 1000;
    if (setsockopt(twoWayUDPSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
        std::cout << "socket timeout set error" << std::endl;
        return 1;
    }

    // send a connection request packet to the server, also sets the NAT translation rule in router between sockets

    // get server address info
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    iResult = getaddrinfo(serveraddr.c_str(), SERVER_UDP_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    serverAddressLen = result->ai_addrlen;
    memcpy(&serverAddress, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    std::cout << "[NETWORK] Attempting to ping server... here we go" << std::endl;
    std::string pong = loopTillResponse(&twoWayUDPSocket, "tryping", 7, (sockaddr*)&serverAddress, serverAddressLen);

    ClientRequestConnectionPacket serverResponse;
    serverResponse.fromString(pong);
    clientID = serverResponse.port;
    netEntManager.self->id = clientID;
    std::cout << "[NETWORK] Response from server! My id is: " << clientID << std::endl;

    std::cout << "[NETWORK] Requesting current server snapshot..." << std::endl;
    std::string snapRequest = std::string("getsnapshot") + std::to_string(clientID);
    std::string snapshot = loopTillResponse(&twoWayUDPSocket, snapRequest.c_str(), snapRequest.length(), (sockaddr*)&serverAddress, serverAddressLen);
    std::cout << "[NETWORK] Response from server!" << std::endl;

    ServerSnapshot sp;
    sp = ServerSnapshot::fromString(snapshot);
    netEntManager.imageFormat = swImageFormat;
    netEntManager.uniformSetLayout = &uniformLayout;
    netEntManager.setupFromServerPacket(sp, clientID);

    connected = true;

    std::thread serverListenThread(&HyacinthNetworkClient::listenForServer, this, twoWayUDPSocket);
    serverListenThread.detach();

    return 0;
}

void HyacinthNetworkClient::updateServerTick(ClientUpdatePacket& p, bool mouseLocked) {
    p.ack = serverAck;
    std::string s = p.toString();
    const char* msg = s.c_str();
    sendto(twoWayUDPSocket, msg, strlen(msg), 0, (sockaddr*)&serverAddress, serverAddressLen);
}

void HyacinthNetworkClient::shutdownNet() {
    netEntManager.shutdown();
}