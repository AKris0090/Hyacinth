#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define PACKET_PORT "6767"

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

#define LAG_SIMULATION

constexpr long long CLIENT_TIMEOUT = 3000;

EntityManager entityManager;
PhysicsManager physicsManager;
ThreadSafeQueue<Event> serverEvents;
LagSimulator lagSim;
std::mutex bufferedClientPacketMutex;
std::queue<ClientUpdatePacket> bufferedClientPackets;
RewindRingBuffer rewindBuffer;
std::atomic<uint32_t> currentClientID{ 0 };
std::atomic<uint32_t> currentTick{ 0 };
std::atomic<std::shared_ptr<ServerSnapshot>> currentSnapshot;

using namespace std::chrono;

bool isSameAddress(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr &&
        a.sin_port == b.sin_port;
}

std::filesystem::path getExeDir()
{
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
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
    int clientAddrSize = sizeof(clientAddr);

    while (true) {
        int bytesReceived = recvfrom(*udpSocket, recvBuff, sizeof(recvBuff) - 1, 0, (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) { // usually client disconnect
            continue;
        }

        recvBuff[bytesReceived] = '\0';
        size_t found = std::string(recvBuff).find("tryping");
        if (found != std::string::npos) {
            Event e;
            e.eventType = SERVER_EVENT::CLIENT_JOIN;
            e.clientAddr = clientAddr;
            e.clientAddrSize = clientAddrSize;
            serverEvents.push(e);
            continue;
        }
        size_t snapRequest = std::string(recvBuff).find("getsnapshot");
        if (snapRequest != std::string::npos) {
            continue;
        }

        ClientUpdatePacket p = ClientUpdatePacket::fromString(std::string(recvBuff));
        // when a packet is received, write its timestamp down to estimate ping
        p.serverTimestamp = getNowMs();

#ifdef LAG_SIMULATION
        std::unique_lock l(lagSim.buffLock);
        std::unique_lock lck(bufferedClientPacketMutex);
        if (p.id == 1) {
            bufferedClientPackets.push(p);
        }
        else {
            lagSim.lagPacketBuffer[currentTick + SERVER_FRAME_LAG].push(p);
        }
#endif
#ifndef LAG_SIMULATION
        std::unique_lock l(bufferedClientPacketMutex);
        bufferedClientPackets.push(p);
#endif
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
            << std::setw(12) << (std::to_string(client->ping) + "ms")
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
            case SERVER_EVENT::CLIENT_JOIN: {
                int existingID = -1;
                for (auto& [id, client] : entityManager.clients) {
                    if (isSameAddress(client->clientAddr, e.clientAddr)) {
                        existingID = id;
                        break;
                    }
                }

                ClientRequestConnectionPacket response;

                if (existingID > -1) {
                    response.port = existingID;
                    std::string respStr = response.toString();
                    sendto(*udpSendSocket, respStr.c_str(), respStr.length(), 0, (sockaddr*)&entityManager.clients[e.clientID]->clientAddr, entityManager.clients[e.clientID]->clientAddrLen);
                    break;
                }
                currentClientID++;

                e.clientID = currentClientID;

                ServersideClient* newClient = new ServersideClient();
                newClient->id = e.clientID;
                newClient->entity.id = e.clientID;
                newClient->heartBeat = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

                entityManager.clients[e.clientID] = newClient;
                physicsManager.addCharacterController(newClient->id);

                entityManager.clients[e.clientID]->clientAddr = e.clientAddr;
                entityManager.clients[e.clientID]->clientAddrLen = e.clientAddrSize;

                response.port = e.clientID;
                std::string respStr = response.toString();
                sendto(*udpSendSocket, respStr.c_str(), respStr.length(), 0, (sockaddr*)&entityManager.clients[e.clientID]->clientAddr, entityManager.clients[e.clientID]->clientAddrLen);

                break;
            }
            case SERVER_EVENT::CLIENT_DISCONNECT:
                entityManager.clients.erase(e.clientID);
                break;
            default:
                break;
            }
        }

        {
            std::unique_lock l(bufferedClientPacketMutex);
#ifdef LAG_SIMULATION
            // scroll through lag sim buffer, and push all packets into bufferedClientPackets
            std::unique_lock lock(lagSim.buffLock);
            if (lagSim.lagPacketBuffer.find(currentTick) != lagSim.lagPacketBuffer.end()) {
                while (lagSim.lagPacketBuffer[currentTick].size() > 0) {
                    ClientUpdatePacket p = lagSim.lagPacketBuffer[currentTick].front();
                    p.serverTimestamp = getNowMs();
                    bufferedClientPackets.push(p);
                    lagSim.lagPacketBuffer[currentTick].pop();
                }
                lagSim.lagPacketBuffer.erase(currentTick);
            }
#endif
            // flush buffered packets
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
            bool loaded = client->getPacketFor(currentTick); // loads the correct packet for this tick into the client's bufferedPacket field
            if (loaded) {
                if (client->foundTimestamp(client->bufferedPacket.ackedTick)) {
                    client->ping = client->bufferedPacket.receivedTimestamp - client->sendTimestamps.front().second;
                }
                else {
                    std::cout << "not found!" << "," << client->bufferedPacket.ackedTick << "," << client->sendTimestamps.front().first << std::endl;
                }
            }
        }
        physicsManager.updatePhysicsServer(&entityManager);

        auto p = std::make_shared<ServerSnapshot>();
        rewindSnapshot r;
        r.tickNum = currentTick;
        for (const auto& [id, client] : entityManager.clients) {
            if (glm::abs(client->bufferedPacket.movementFB) > 0.f || glm::abs(client->bufferedPacket.movementLR) > 0.f) {
                client->entity.isMoving = true;
            }
            else {
                client->entity.isMoving = false;
            }

            bool canShoot = client->entity.pistolController.updateShooting(SERVER_TIMESTEP, client->bufferedPacket.shooting);

            hitReg h;
            if (canShoot) {
                // usually, it would be Current Server Time - Packet Latency - Client View Interpolation. In this case, RTT / 2 = 0 because everything is being run locally.
                // TODO: find a way to estimate the client's ping. By figuring that out, further subtract that from tickRewind. 
                uint32_t tickRewind = currentTick - SERVER_INPUT_BUFFER - (client->ping / SERVER_TIMESTEP_MS.count()); // client ping divided by 
                rewindSnapshot r = rewindBuffer.getSnapshotFromTick(tickRewind); 
                if (r.tickNum == INT_MAX) { // couldnt find snapshot in the buffer
                    std::cout << "couldn't find the right snapshot, too far in the past" << std::endl;
                }
                else {
                    h = physicsManager.playerShooting(client->id, client->entity.transform, &r);
                    if (h.hit) {
                        // std::cout << "entity: " << id << " has hit client: " << h.entityHitId << std::endl << std::endl;
                        // client->entity.shotAck = true;
                    }
                    else {
                        // std::cout << "airball" << std::endl << std::endl;
                    }
                }
            }

            client->bufferedPacket.reset();
            p->entities.push_back(client->entity);

#ifdef LAG_SIMULATION
            if (canShoot && h.hit) {
                for (auto& ent : p->entities) {
                    if (ent.id == client->id) {
                        ent.shotAck = true;
                    }
                    if (ent.id == 1) {
                        ent.transform.position = h.footPosHit;
                    }
                }
            }
#endif
            r.entityPositions.push_back(entityPositionSnapshot{ id, client->entity.transform.position });
        }
        rewindBuffer.push(r);
        p->serverTickNum = currentTick;
        for (const auto& [id, client] : entityManager.clients) {
            p->processedTickNum = currentTick - client->tickBasis;
            if (client->tickBasis > currentTick) continue;
            client->sendTimestamps.push({ currentTick, getNowMs() });
            std::string packetString = p->toString();
            sendto(*udpSendSocket, packetString.c_str(), packetString.length(), 0, (sockaddr*)&client->clientAddr, client->clientAddrLen);
        }
        currentSnapshot.store(p, std::memory_order_release);

        // printPhysicsTick();

        std::this_thread::sleep_until(nextTick);
        currentTick++;
    }
}

int main()
{
    SOCKET udpTwoWaySocket = INVALID_SOCKET;

    // setup physics with base scene as a static mesh
    {
        LightLoader loader;
        auto path = getExeDir() / "objects" / "test_scene.glb";

        physicsManager.initPhysics(true);
        physicsManager.addStaticPhysicsObject(loader.loadFromFile(path.string(), true));
    }

    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "[NETWORK] WSA startup failed: " << iResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, hints;

    // setup and bind udp listener (and sender) socket
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
    udpTwoWaySocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (udpTwoWaySocket == INVALID_SOCKET) {
        std::cout << "[NETWORK] problem with udp socket(): " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    iResult = bind(udpTwoWaySocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult != 0) {
        std::cout << "[NETWORK] udp bind failed: " << iResult << std::endl;
        freeaddrinfo(result);
        closesocket(udpTwoWaySocket);
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
        std::cout << "[NETWORK] Connect from other devices using: " << localIP << ":" << PACKET_PORT << std::endl << std::endl;
        freeaddrinfo(localResult);
    }

    // udp listener thread
    std::thread udpSocketThread(serverListenForUDPPackets, &udpTwoWaySocket);

    // physics tick thread. Uses same socket as listener thread to avoid NAT rule setting issue
    std::thread tickThread(updateTick, &udpTwoWaySocket);

    // watchdog thread to manage client timouts and chase them off the lawn
    std::thread watchdogThread(timeoutWatchdog);

    tickThread.join();
    udpSocketThread.join();
    watchdogThread.join();

    WSACleanup();
    return 0;
}