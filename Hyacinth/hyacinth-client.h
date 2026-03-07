#pragma once

#define DEFAULT_PORT "6767"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>
#include <string>

class HyacinthNetworkClient {
public:
	int setup(std::string serveraddr);
};