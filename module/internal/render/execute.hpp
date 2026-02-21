#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

void execute_script_async(std::string script) {
    if (script.empty() || script.find_first_not_of(" \t\n\r") == std::string::npos) {
        return;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    DWORD timeout = 3000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(6969);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != SOCKET_ERROR) {
        uint32_t len = htonl(static_cast<uint32_t>(script.length()));

        send(sock, reinterpret_cast<const char*>(&len), sizeof(len), 0);

        send(sock, script.c_str(), script.length(), 0);
    }

    closesocket(sock);
    WSACleanup();
}