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

EntityManager entityManager;
PhysicsManager physicsManager;
SPSCQueue<Event> serverEvents;
std::mutex bufferedClientPacketMutex;
std::queue<ClientUpdatePacket> bufferedClientPackets;
std::atomic<uint32_t> currentClientID{ 0 };
std::atomic<uint32_t> currentTick{ 0 };
std::atomic<std::shared_ptr<ServerSnapshot>> currentSnapshot;

using namespace std::chrono;

std::filesystem::path getExeDir()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

void handleNewClient(SOCKET socket, ServersideClient* newClient) {
    SetThreadDescription(GetCurrentThread(), L"ClientHandler");

    int initialReq, serverAck, entityMessage;

    char recvbuf[DEFAULT_LEN];
    int recvbuflen = DEFAULT_LEN;
    initialReq = recv(socket, recvbuf, recvbuflen, 0);

    if (initialReq > 0) {
        recvbuf[initialReq] = '\0';
        ClientRequestConnectionPacket p;
        p.fromString(std::string(recvbuf));

        ClientRequestConnectionPacket response;
        response.port = newClient->id;
        response.tick = currentTick;

        std::string msg = response.toString();
        serverAck = send(socket, msg.c_str(), msg.length(), 0);
        if (serverAck == SOCKET_ERROR) {
            std::cout << "acknowledge failed to send?" << std::endl;
            closesocket(socket);
            return;
        }

        std::string spString = currentSnapshot.load(std::memory_order_acquire)->toString();
        entityMessage = send(socket, spString.c_str(), spString.length(), 0);
        if (entityMessage == SOCKET_ERROR) {
            std::cout << "[NETWORK] entityList failed to send?" << std::endl;
            closesocket(socket);
            entityManager.clients.erase(newClient->id);
            return;
        }

        shutdown(socket, SD_BOTH);

        Event e;
        e.eventType = SERVER_EVENT::CLIENT_JOIN;
        e.newClient = newClient;
        e.clientID = newClient->id;
        serverEvents.push(e);
    }
    else {
        std::cout << "[NETWORK] client initiation receive failure: " << initialReq << " " << WSAGetLastError() << std::endl;
        closesocket(socket);
    }
}

void serverListenForClients(SOCKET* tcpSocket) {
    SetThreadDescription(GetCurrentThread(), L"NewClientListener");
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

        std::shared_lock lok(entityManager.clientsMutex);
        if (entityManager.clients.size() > MAX_CONNECTIONS) {
            std::cout << "[NETWORK] Cannot accept new client, max number of connections exceeded!" << std::endl;
            continue;
        }

        currentClientID++;
        ServersideClient* newClient = new ServersideClient();
        newClient->id = currentClientID;
        newClient->entity.id = currentClientID;
        newClient->heartBeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

        std::thread newClientThread(handleNewClient, clientSocket, newClient);
        newClientThread.join();
    }
}

void timeoutWatchdog() {
    SetThreadDescription(GetCurrentThread(), L"TimeoutWatchdog");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        std::shared_lock lock(entityManager.clientsMutex);
        long long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
        std::vector<uint32_t> idsToErase;
        for (auto& [id, client] : entityManager.clients) {
            if (now - client->heartBeat > CLIENT_TIMEOUT) {
                idsToErase.push_back(id);
            }
        }
        for (auto& id : idsToErase) {
            Event e;
            e.clientID = id;
            e.eventType = SERVER_EVENT::CLIENT_DISCONNECT;
            physicsManager.physicsEventQueue.push(e);
            serverEvents.push(e);
        }
    }
}

void serverListenForUDPPackets(SOCKET* udpSocket) {
    SetThreadDescription(GetCurrentThread(), L"UDPListener");

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
        size_t found = std::string(recvBuff).find("tryping");
        if (found != std::string::npos) {
            Event e;
            e.eventType = SERVER_EVENT::CLIENT_UPDATE_ADDR;
            e.clientID = std::stoi(std::string(recvBuff).substr(found + 7));
            e.clientAddr = clientAddr;
            e.clientAddrSize = clientAddrSize;
            serverEvents.push(e);
            continue;
        }

        ClientUpdatePacket p = ClientUpdatePacket::fromString(std::string(recvBuff));
        std::unique_lock l(bufferedClientPacketMutex);
        bufferedClientPackets.push(p);
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
            << std::setw(12) << "Ping"
            << std::setw(22) << "Tick offset"
            << "\n";
        std::cout << std::string(60, '-') << "\n";
    }
    firstPrint = false;

    long long now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    for (auto& [id, client] : entityManager.clients) {
        char ipbuff[16];
        inet_ntop(AF_INET, (void*)&client->clientAddr.sin_addr, ipbuff, 16);

        std::string ipPort = std::string(ipbuff) + ":" + std::to_string(ntohs(client->clientAddr.sin_port));

        std::cout << std::left
            << std::setw(6) << id
            << std::setw(20) << ipPort
            << std::setw(12) << (std::to_string(client->heartBeat) + "ms")
            << std::setw(22) << (std::to_string(client->tickBasis))
            << "\n";
    }

    prevLinesToClear = entityManager.clients.size();

    std::cout.flush();
}

void updateTick(SOCKET* udpSendSocket) {
    SetThreadDescription(GetCurrentThread(), L"SimulationTick");
    auto epoch = std::chrono::steady_clock::now();

    while (true) {
        auto nextTick = epoch + (currentTick + 1) * SERVER_TIMESTEP_MS;
        
        // handle all server events
        Event e;
        while (serverEvents.pop(e)) {
            std::unique_lock lok(entityManager.clientsMutex);
            switch (e.eventType) {
            case SERVER_EVENT::CLIENT_JOIN:
                entityManager.clients[e.clientID] = e.newClient;
                physicsManager.addCharacterController(e.newClient->id);
                break;
            case SERVER_EVENT::CLIENT_DISCONNECT:
                entityManager.clients.erase(e.clientID);
                break;
            case SERVER_EVENT::CLIENT_UPDATE_ADDR:
                if (entityManager.clients.find(e.clientID) != entityManager.clients.end()) {
                    entityManager.clients[e.clientID]->clientAddr = e.clientAddr;
                    entityManager.clients[e.clientID]->clientAddrLen = e.clientAddrSize;
                    entityManager.clients[e.clientID]->addressSet = true;

                    sendto(*udpSendSocket, "pong", 4, 0, (sockaddr*)&entityManager.clients[e.clientID]->clientAddr, entityManager.clients[e.clientID]->clientAddrLen);
                }

                break;
            default:
                break;
            }
        }

        // flush buffered packets
        {
            std::unique_lock l(bufferedClientPacketMutex);
            while (!bufferedClientPackets.empty()) {
                ClientUpdatePacket p = bufferedClientPackets.front();
                if (entityManager.clients.find(p.id) != entityManager.clients.end()) {
                    auto& client = entityManager.clients[p.id];

                    if (!client->tickOffsetSet) {
                        client->tickBasis = currentTick + client->tickBasisOffset; // reducing jitter for packets arriving at server
                        client->tickOffsetSet = true;
                    }
                    client->clientPacketBuffer.push(p);
                    client->heartBeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                }
                bufferedClientPackets.pop();
            }
        }

        for (const auto& [id, client] : entityManager.clients) {
            client->getPacketFor(currentTick);
        }
        physicsManager.updatePhysicsServer(&entityManager);

        auto p = std::make_shared<ServerSnapshot>();
        for (const auto& [id, client] : entityManager.clients) {
            if (glm::abs(client->bufferedPacket.movementFB) > 0.f || glm::abs(client->bufferedPacket.movementLR) > 0.f) {
                client->entity.isMoving = true;
            }
            else {
                client->entity.isMoving = false;
            }

            if (client->bufferedPacket.shooting) {
                hitReg h = physicsManager.playerShooting(client->id, client->entity.transform, p.get());
                if (h.hit) {
                    std::cout << "entity: " << id << " has hit client: " << h.entityHitId << std::endl;
                    client->entity.shotAck = true;
                }
                else {
                    std::cout << "airball" << std::endl;
                }
            }

            client->bufferedPacket.reset();
            p->entities.push_back(client->entity);
        }
        for (const auto& [id, client] : entityManager.clients) {
            if (!client->addressSet) continue;
            p->processedTickNum = currentTick - client->tickBasis;
            if (client->tickBasis > currentTick) continue;
            std::string packetString = p->toString();
            sendto(*udpSendSocket, packetString.c_str(), packetString.length(), 0, (sockaddr*)&client->clientAddr, client->clientAddrLen);
        }
        currentSnapshot.store(p, std::memory_order_release);

        // printPhysicsTick();

        std::this_thread::sleep_until(nextTick);
        currentTick++;
    }
}

void startPhysics() {
    LightLoader loader;
    auto path = getExeDir() / "objects" / "sponza" / "sponza.gltf";

    physicsManager.initPhysics(true);
    physicsManager.addStaticPhysicsObject(loader.loadFromFile(path.string(), true));
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

    std::thread tickThread(updateTick, &udpListenSocket);

    std::thread watchdogThread(timeoutWatchdog);

    tickThread.join();
    tcpSocketThread.join();
    udpSocketThread.join();
    watchdogThread.join();

    WSACleanup();
    return 0;
}