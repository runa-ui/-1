#pragma once
// 第3回の補助処理。授業ではmainの流れを先に確認する。
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr std::size_t MAX_BYTES = 64 * 1024;

inline void fail(const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}

// スコープを抜けるとcloseする。コピーで二重closeしないよう禁止。
struct Socket {
    int fd;
    explicit Socket(int value) : fd(value) {
        if (fd < 0) fail("socket/accept");
    }
    ~Socket() { ::close(fd); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};

inline sockaddr_in loopbackAddress(unsigned short port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1)
        throw std::runtime_error("inet_pton failed");
    return address;
}

// sendの成功は「相手のアプリが処理した」という意味ではない。
inline void sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = send(fd, data.data() + sent, data.size() - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            fail("send");
        }
        if (n == 0) throw std::runtime_error("send made no progress");
        sent += static_cast<std::size_t>(n);
    }
}

// 今回はEOF（相手の送信終了）を1件の終端とする。
// バッファを文字列と決めつけず、受信した長さnを使う。
inline std::string receiveUntilEof(int fd, std::size_t bufferSize, bool logChunks) {
    if (bufferSize == 0 || bufferSize > MAX_BYTES)
        throw std::runtime_error("invalid buffer size");
    std::vector<char> buffer(bufferSize);
    std::string data;
    for (;;) {
        const ssize_t n = recv(fd, buffer.data(), buffer.size(), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            fail("recv");
        }
        if (n == 0) return data;
        if (data.size() + static_cast<std::size_t>(n) > MAX_BYTES)
            throw std::runtime_error("message exceeds 64 KiB");
        if (logChunks) std::cout << "recv: " << n << " bytes\n";
        data.append(buffer.data(), static_cast<std::size_t>(n));
    }
}
