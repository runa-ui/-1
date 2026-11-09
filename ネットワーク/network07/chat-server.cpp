// Network Systems 07: multi-user chat server (macOS / Linux / WSL, C++17).
// Extends 06: each connection has a worker thread. JOIN/SAY/QUIT keep their meaning.
// SAY is now broadcast to all JOINed clients, including the sender.
// INFO JOIN/LEAVE are notifications. RENAME, USERS and HISTORY are optional tasks.
#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int PORT = 8081;
constexpr std::size_t MAX_LINE_BYTES = 256;
constexpr std::size_t MAX_NAME_BYTES = 16;
constexpr std::size_t MAX_TEXT_BYTES = 200;

void fail(const char* what) {
    std::perror(what);
    std::exit(EXIT_FAILURE);
}

int createServerSocket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) fail("socket");
    int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        fail("setsockopt");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // This PC only.
    address.sin_port = htons(PORT);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        fail("bind");
    if (listen(fd, 16) < 0) fail("listen");
    return fd;
}

// TCP is a stream: one recv() may contain half a line or several lines.
enum class ReadResult { Line, Closed, TooLong, Error };
ReadResult receiveLine(int fd, std::string& pending, std::string& line) {
    for (;;) {
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
        ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
        if (n > 0) pending.append(buffer, static_cast<std::size_t>(n));
        else if (n == 0) return ReadResult::Closed;
        else if (errno != EINTR) return ReadResult::Error;
    }
}

bool validName(const std::string& value) {
    if (value.empty() || value.size() > MAX_NAME_BYTES) return false;
    for (unsigned char c : value)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_'))
            return false;
    return true;
}

bool validText(const std::string& value) {
    if (value.empty() || value.size() > MAX_TEXT_BYTES) return false;
    for (unsigned char c : value)
        if (c < 0x20 || c == 0x7F) return false;
    return true;
}

// Shared data (new in 07). Only JOINed clients are in the list.
struct Client {
    explicit Client(int fd) : sock(fd) {}
    int sock;
    std::mutex sendMutex; // Prevent mixed lines and close/send races on THIS socket.
};

std::mutex clientsMutex; // Protects additions and removals to the shared list.
std::vector<std::shared_ptr<Client>> clients;
std::mutex logMutex;

// Send an entire line. Holding the per-client mutex prevents two workers
// from interleaving their bytes on the same TCP socket.
bool sendLine(const std::shared_ptr<Client>& c, const std::string& line) {
    std::lock_guard<std::mutex> guard(c->sendMutex);
    if (c->sock < 0) return false;
    const std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(c->sock, data.data() + sent, data.size() - sent, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

// The lock is held only long enough to copy the list.
// Never hold clientsMutex while send() can block.
void broadcast(const std::string& line) {
    std::vector<std::shared_ptr<Client>> receivers;
    {
        std::lock_guard<std::mutex> guard(clientsMutex);
        receivers = clients;
    }
    for (const auto& c : receivers) sendLine(c, line);
}

void addClient(const std::shared_ptr<Client>& c) {
    std::lock_guard<std::mutex> guard(clientsMutex);
    clients.push_back(c);
}

void removeClient(const std::shared_ptr<Client>& c) {
    std::lock_guard<std::mutex> guard(clientsMutex);
    clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
}

void closeClient(const std::shared_ptr<Client>& c) {
    std::lock_guard<std::mutex> guard(c->sendMutex);
    if (c->sock >= 0) {
        shutdown(c->sock, SHUT_RDWR);
        close(c->sock);
        c->sock = -1;
    }
}

void logEvent(int id, const char* event) {
    std::lock_guard<std::mutex> guard(logMutex);
    std::cout << "Client #" << id << " " << event << std::endl;
}

// Each connection has its own thread, pending text, and JOIN state.
void handleClient(const std::shared_ptr<Client>& c, int id) {
    logEvent(id, "connected");
    std::string pending;
    std::string name; // Empty until JOIN succeeds.
    bool joined = false;

    for (;;) {
        std::string line;
        ReadResult result = receiveLine(c->sock, pending, line);
        if (result == ReadResult::Closed || result == ReadResult::Error) break;
        if (result == ReadResult::TooLong) {
            sendLine(c, "ERR TOO_LONG");
            break;
        }

        if (line == "QUIT") {
            sendLine(c, "BYE");
            break;
        }
        if (line.empty()) {
            if (!sendLine(c, "ERR EMPTY")) break;
        } else if (line == "JOIN" || line == "SAY" ||
                   line.rfind("QUIT ", 0) == 0) {
            if (!sendLine(c, "ERR BAD_FORMAT")) break;
        } else if (line.rfind("JOIN ", 0) == 0) {
            if (joined) {
                if (!sendLine(c, "ERR ALREADY_JOINED")) break;
                continue;
            }
            std::string requested = line.substr(5);
            if (!validName(requested)) {
                if (!sendLine(c, "ERR BAD_NAME")) break;
                continue;
            }
            // First acknowledge JOIN, then include the user in broadcasts.
            if (!sendLine(c, "OK JOIN " + requested)) break;
            name = requested;
            joined = true;
            addClient(c);
            broadcast("INFO JOIN " + name);
        } else if (line.rfind("SAY ", 0) == 0) {
            if (!joined) {
                if (!sendLine(c, "ERR NOT_JOINED")) break;
                continue;
            }
            std::string text = line.substr(4);
            if (text.size() > MAX_TEXT_BYTES) {
                if (!sendLine(c, "ERR TEXT_TOO_LONG")) break;
                continue;
            }
            if (!validText(text)) {
                if (!sendLine(c, "ERR BAD_FORMAT")) break;
                continue;
            }
            // No separate ACK: the MSG sent to yourself is also the response.
            broadcast("MSG " + name + " " + text);
        } else {
            if (!sendLine(c, "ERR UNKNOWN_COMMAND")) break;
        }
    }

    if (joined) removeClient(c);
    closeClient(c);
    if (joined) broadcast("INFO LEAVE " + name);
    logEvent(id, "disconnected");
}

int main() {
    std::signal(SIGPIPE, SIG_IGN); // Keep running if a client disconnects.
    int serverFd = createServerSocket();
    std::cout << "Chat server listening at 127.0.0.1:" << PORT << '\n';
    int nextId = 0; // Used only in the accept thread.

    for (;;) {
        int sock = accept(serverFd, nullptr, nullptr);
        if (sock < 0) {
            if (errno == EINTR) continue;
            std::perror("accept");
            break;
        }
        auto client = std::make_shared<Client>(sock);
        std::thread(handleClient, client, ++nextId).detach();
    }
    close(serverFd);
    return 0;
}
