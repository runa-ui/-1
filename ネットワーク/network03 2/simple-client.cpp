// 第3回 基本版：短い文字列を送り、1回受信する。macOS / Linux / WSL用。
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024; // 観察では4に変更。観察後は戻す。
constexpr std::size_t MAX_INPUT_BYTES = 1024; // 文字数ではなくバイト数

void fail(const char* operation) {
    std::perror(operation);
    std::exit(EXIT_FAILURE);
}

int createClientSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) fail("socket");
    return sock;
}

void connectToServer(int sock, const char* server_ip, int port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address.\n";
        std::exit(EXIT_FAILURE);
    }
    if (connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        fail("connect");
}

void sendMessage(int sock, const std::string& message) {
    ssize_t sent = send(sock, message.data(), message.size(), 0);
    if (sent < 0) fail("send");
    if (static_cast<std::size_t>(sent) != message.size()) {
        std::cerr << "Partial send: retrying the rest is not implemented.\n";
        std::exit(EXIT_FAILURE);
    }
}

ssize_t receiveMessage(int sock, char* buffer, int size) {
    return recv(sock, buffer, size, 0); // 1回だけ受信。続きは読み直さない。
}

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    int sock = createClientSocket();
    connectToServer(sock, "127.0.0.1", PORT);

    std::string message;
    while (true) {
        std::cout << "Enter message: " << std::flush;
        if (!std::getline(std::cin, message)) { close(sock); return 0; }
        if (!message.empty() && message.size() <= MAX_INPUT_BYTES) break;
        std::cout << "Enter 1 to 1024 bytes (empty input is not sent).\n";
    }
    sendMessage(sock, message);

    char buffer[BUFFER_SIZE];
    ssize_t received = receiveMessage(sock, buffer, BUFFER_SIZE);
    if (received < 0) fail("recv");
    if (received > 0) {
        std::cout << "Received " << received << " bytes: ";
        std::cout.write(buffer, received);
        std::cout << std::endl;
    } else {
        std::cout << "Peer finished without a reply." << std::endl;
    }
    close(sock);
}
