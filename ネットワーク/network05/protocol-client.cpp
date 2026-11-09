// 第5回：行単位の通信仕様を試すクライアント（1接続で複数の要求を送る）。
// macOS / Linux / WSL用。第6・7回で複数人チャットへ拡張します。
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

constexpr int PORT = 8081;
constexpr std::size_t MAX_LINE_BYTES = 256;

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

// 第5回追加：最後に改行を付け、途中までしか送れなければ残りも送る。
bool sendMessage(int sock, const std::string& line) {
    const std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(sock, data.data() + sent, data.size() - sent, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            std::perror("send");
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

enum class ReadResult { Line, Closed, TooLong, Error };

// 第5回追加：recvが途中で区切れても '\n' まで読み、余りは残す。
ReadResult receiveLine(int sock, std::string& pending, std::string& line) {
    while (true) {
        std::size_t pos = pending.find('\n');
        if (pos != std::string::npos) {
            if (pos > MAX_LINE_BYTES) return ReadResult::TooLong;
            line = pending.substr(0, pos);
            pending.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return ReadResult::Line;
        }
        if (pending.size() > MAX_LINE_BYTES) return ReadResult::TooLong;

        char buffer[1024];
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
        if (n > 0) {
            pending.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0) {
            return ReadResult::Closed;
        } else if (errno != EINTR) {
            std::perror("recv");
            return ReadResult::Error;
        }
    }
}

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    int sock = createClientSocket();
    connectToServer(sock, "127.0.0.1", PORT);
    std::cout << "Connected to 127.0.0.1:" << PORT << std::endl;
    std::cout << "Commands: JOIN name / SAY text / QUIT\n";

    std::string pending; // 受信途中のデータ
    std::string input;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) break;
        if (input.size() > 1024) {
            std::cout << "Input is too long (max 1024 bytes).\n";
            continue;
        }
        if (!sendMessage(sock, input)) break;

        std::string reply;
        ReadResult result = receiveLine(sock, pending, reply);
        if (result != ReadResult::Line) {
            std::cout << "Disconnected before a complete response.\n";
            break;
        }
        std::cout << "< " << reply << std::endl;
        if (reply == "BYE" || reply == "ERR TOO_LONG") break;
    }
    close(sock);
}
