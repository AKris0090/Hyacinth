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
#include <iomanip>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include "glm/glm.hpp"

#include "hyacinth_network.h"
#include "hyacinth_physics.h"
#include "light_loader.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Hyacinth-Common.lib")
#pragma comment(lib, "Hyacinth-Physics.lib")

constexpr long long CLIENT_TIMEOUT = 3000;

uint32_t currentClientID = -1;
EntityManager entityManager;
std::mutex entityManagerMutex;
std::vector<LightObject*> staticPhysicsObjects;
PhysicsManager physicsManager;
int currentNumConnections = 0;

std::filesystem::path getExeDir()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

void handleNewClient(SOCKET socket, ServersideClient* newClient) {
    int initialReq, serverAck, entityMessage;

    char recvbuf[DEFAULT_LEN];
    int recvbuflen = DEFAULT_LEN;
    initialReq = recv(socket, recvbuf, recvbuflen, 0);

    if (initialReq > 0) {
        recvbuf[initialReq] = '\0';
        ClientRequestConnectionPacket p;
        p.fromString(std::string(recvbuf));

        newClient->clientAddr.sin_port = htons(p.port);

        ClientRequestConnectionPacket response;
        response.port = newClient->id;

        std::string msg = response.toString();
        serverAck = send(socket, msg.c_str(), msg.length(), 0);
        if (serverAck == SOCKET_ERROR) {
            std::cout << "acknowledge failed to send?" << std::endl;
            closesocket(socket);
            entityManager.clients.erase(newClient->id);
            return;
        }

        ServerPacket sp;
        for (const auto& [id, client] : entityManager.clients) {
            sp.entities.push_back(client->entity);
        }
        std::string spString = sp.toString();
        entityMessage = send(socket, spString.c_str(), spString.length(), 0);
        if (entityMessage == SOCKET_ERROR) {
            std::cout << "[NETWORK] entityList failed to send?" << std::endl;
            closesocket(socket);
            entityManager.clients.erase(newClient->id);
            return;
        }

        shutdown(socket, SD_BOTH);

        physicsManager.addCharacterController(newClient->id);
        currentNumConnections++;
    }
    else {
        std::cout << "[NETWORK] client initiation receive failure: " << initialReq << " " << WSAGetLastError() << std::endl;
        entityManager.clients.erase(newClient->id);
        closesocket(socket);
    }
}

void serverListenForClients(SOCKET* tcpSocket) {
    while (true) {
        if (listen(*tcpSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cout << "[NETWORK] Listen failed with error: " << WSAGetLastError() << std::endl;
            closesocket(*tcpSocket);
            WSACleanup();
            return;
        }

        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);

        SOCKET clientSocket = INVALID_SOCKET;
        clientSocket = accept(*tcpSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "[NETWORK] accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        if (currentNumConnections + 1 > MAX_CONNECTIONS) {
            std::cout << "[NETWORK] Cannot accept new client, max number of connections exceeded!" << std::endl;
            continue;
        }

        currentClientID++;
        ServersideClient* newClient = new ServersideClient();
        newClient->id = currentClientID;
        newClient->entity.id = currentClientID;
        newClient->clientAddr = clientAddr;
        newClient->clientAddrLen = clientAddrSize;
        entityManager.clients[currentClientID] = newClient;

        std::thread newClientThread(handleNewClient, clientSocket, newClient);
        newClientThread.detach();
    }
}

void timeoutWatchdog() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        using namespace std::chrono;
        long long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        entityManagerMutex.lock();
        std::vector<uint32_t> idsToErase;
        for (auto& [id, client] : entityManager.clients) {
            if (now - client->heartBeat > CLIENT_TIMEOUT) {
                idsToErase.push_back(id);
            }
        }
        for (auto& id : idsToErase) {
            physicsManager.removeCharacterController(id);
            entityManager.clients.erase(id);
        }
        entityManagerMutex.unlock();
    }
}

void serverListenForUDPPackets(SOCKET* udpSocket) {
    char recvBuff[DEFAULT_LEN];
    sockaddr_in clientAddr;

    while (true) {
        int clientAddrSize = sizeof(clientAddr);

        int bytesReceived = recvfrom(*udpSocket, recvBuff, sizeof(recvBuff) - 1, 0, (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cout << "[NETWORK] recvfrom failed: " << WSAGetLastError() << std::endl;
            break;
        }

        recvBuff[bytesReceived] = '\0';

        ClientUpdatePacket p = ClientUpdatePacket::fromString(std::string(recvBuff));
        entityManagerMutex.lock();
        if ((entityManager.clients.find(p.id) != entityManager.clients.end()) && (entityManager.clients[p.id] != NULL)) {
            entityManager.clients[p.id]->bufferedPackets.addPacket(p);
            using namespace std::chrono;
            entityManager.clients[p.id]->heartBeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        }
        entityManagerMutex.unlock();
    }

    closesocket(*udpSocket);
}

bool firstPrint = true;
int prevLinesToClear = 0;

void printPhysicsTick() {
    if (!firstPrint) {
        for (int i = 0; i < prevLinesToClear; i++) {
            std::cout << "\033[A\033[2K";
        }
    }
    else {
        std::cout << "=== SERVER STATUS ===\n";
        std::cout << std::left
            << std::setw(6) << "ID"
            << std::setw(20) << "Address"
            << std::setw(12) << "Last Seen"
            << "\n";
        std::cout << std::string(48, '-') << "\n";
    }
    firstPrint = false;

    using namespace std::chrono;
    long long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    for (auto& [id, client] : entityManager.clients) {
        long long msSince = now - client->heartBeat;
        char ipbuff[16];
        inet_ntop(AF_INET, (void*)&client->clientAddr.sin_addr, ipbuff, 16);

        std::string ipPort = std::string(ipbuff) + ":" + std::to_string(ntohs(client->clientAddr.sin_port));

        std::cout << std::left
            << std::setw(6) << id
            << std::setw(20) << ipPort
            << std::setw(12) << (std::to_string(msSince) + "ms")
            << "\n";
    }

    prevLinesToClear = entityManager.clients.size();

    std::cout.flush();
}

void updateTick() {
    using namespace std::chrono_literals;

    SOCKET serverSendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    while (true) {
        physicsManager.updatePhysics(&entityManager);

        entityManagerMutex.lock();
        for (const auto& [id, client] : entityManager.clients) {
            client->bufferedPackets.reset();
        }

        ServerPacket p;
        for (const auto& [id, client] : entityManager.clients) {
            p.entities.push_back(client->entity);
        }
        std::string packetString = p.toString();
        for (const auto& [id, client] : entityManager.clients) {
            sendto(serverSendSocket, packetString.c_str(), packetString.length(), 0, (sockaddr*)&client->clientAddr, client->clientAddrLen);
        }
        entityManagerMutex.unlock();
        printPhysicsTick();
        std::this_thread::sleep_for(7.1825ms);
    }
}

void startPhysics() {
    LightLoader loader;
    auto path = getExeDir() / "objects" / "sponza" / "sponza.gltf";
    staticPhysicsObjects.push_back(loader.loadFromFile(path.string(), true));

    physicsManager.initPhysics();
    physicsManager.addStaticPhysicsObject(staticPhysicsObjects[0]);
}

int main()
{
    startPhysics(); 

    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "[NETWORK] WSA startup failed: " << iResult << std::endl;
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
        std::cout << "[NETWORK] getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    SOCKET tcpListenSocket = INVALID_SOCKET;
    tcpListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (tcpListenSocket == INVALID_SOCKET) {
        std::cout << "[NETWORK] problem with tcp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(tcpListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "[NETWORK] tcp bind failed: " << iResult << std::endl;
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
        std::cout << "[NETWORK] getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return 1;
    }
    SOCKET udpListenSocket = INVALID_SOCKET;
    udpListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (udpListenSocket == INVALID_SOCKET) {
        std::cout << "[NETWORK] problem with udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(udpListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "[NETWORK] udp bind failed: " << iResult << std::endl;
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
        std::cout << "[NETWORK] Connect from other devices using: " << localIP << ":" << DEFAULT_PORT << std::endl << std::endl;
        freeaddrinfo(localResult);
    }

    // listen on the udp thread
    std::thread udpSocketThread(serverListenForUDPPackets, &udpListenSocket);

    std::thread tickThread(updateTick);

    std::thread watchdogThread(timeoutWatchdog);

    tickThread.join();
    tcpSocketThread.join();
    udpSocketThread.join();
    watchdogThread.join();

    WSACleanup();
    return 0;
}