// 第4回：第3回のクライアントに「受信タイムアウト・再試行」を追加。macOS / Linux / WSL用。
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
// 第4回追加：時間計測・再試行前の待機・受信タイムアウトに使う。
#include <chrono>
#include <thread>
#include <cerrno>
#include <sys/time.h>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;
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

// 第4回追加：コマンド引数が指定範囲の整数か確認する。
int number(const char* text, int maximum) {
    try {
        std::size_t end;
        int n = std::stoi(text, &end);
        if (text[end] != '\0' || n < 0 || n > maximum) throw 0;
        return n;
    } catch (...) {
        std::cerr << "Invalid number.\n";
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {
    // 第4回追加：受信待ちの制限時間と最大試行回数（初回を含む）。
    if (argc > 3) {
        std::cerr << "Usage: ./timeout-client [timeout_ms] [attempts]\n";
        return 1;
    }
    const int timeout_ms = argc > 1 ? number(argv[1], 10000) : 1000;
    const int attempts = argc > 2 ? number(argv[2], 3) : 1;
    if (timeout_ms == 0 || attempts == 0) {
        std::cerr << "Use positive numbers.\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN);
    std::string message;
    while (true) {
        std::cout << "Enter message: " << std::flush;
        if (!std::getline(std::cin, message)) return 0;
        if (!message.empty() && message.size() <= MAX_INPUT_BYTES) break;
        std::cout << "Enter 1 to 1024 bytes (empty input is not sent).\n";
    }

    // 第4回追加：時間切れの場合だけ、同じ入力を新しい接続で再送する。
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        int sock = createClientSocket();
        connectToServer(sock, "127.0.0.1", PORT);

        // 第4回追加：recvだけに適用する受信タイムアウト。
        timeval limit{};
        limit.tv_sec = timeout_ms / 1000;
        limit.tv_usec = (timeout_ms % 1000) * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit)) < 0)
            fail("setsockopt");

        std::cout << "Attempt " << attempt << '/' << attempts << std::endl;
        const auto start = std::chrono::steady_clock::now();
        sendMessage(sock, message);

        char buffer[BUFFER_SIZE];
        ssize_t received = receiveMessage(sock, buffer, BUFFER_SIZE);
        const int receive_error = errno; // 第4回追加：表示前にrecvのエラー番号を保存。
        const double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "Elapsed: " << elapsed_ms << " ms\n";

        if (received > 0) {
            std::cout << "Received " << received << " bytes: ";
            std::cout.write(buffer, received);
            std::cout << std::endl;
            close(sock);
            return 0;
        }

        // 第4回追加：時間切れ／相手の切断／その他のエラーを区別する。
        const bool timed_out = received < 0 &&
            (receive_error == EAGAIN || receive_error == EWOULDBLOCK);
        if (timed_out) {
            std::cout << "Receive timeout: server result is unknown." << std::endl;
        } else if (received == 0) {
            std::cout << "Peer closed without reply." << std::endl;
        } else {
            errno = receive_error;
            std::perror("recv");
        }
        close(sock);

        if (!timed_out) return 1; // 再試行するのは受信タイムアウトだけ。
        if (attempt < attempts) {
            std::cout << "Retry after 1000 ms (same message)." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    std::cout << "Attempt limit reached.\n";
    return 2;
}