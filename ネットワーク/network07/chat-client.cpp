// Network Systems 07: chat client with an independent receive thread (C++17).
// JOIN name -> OK JOIN; SAY text -> MSG to all; QUIT -> BYE.
// Plain text automatically becomes SAY text; /quit is also accepted.
// macOS / Linux / WSL; local 127.0.0.1 server, PORT 8081.
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int PORT = 8081;
constexpr std::size_t MAX_LINE_BYTES = 256;
constexpr std::size_t MAX_INPUT_BYTES = MAX_LINE_BYTES;

void fail(const char* what) {
    std::perror(what);
    std::exit(EXIT_FAILURE);
}

int connectToServer() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) fail("socket");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) fail("inet_pton");
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        fail("connect");
    return sock;
}

bool sendLine(int sock, const std::string& line) {
    std::string data = line + "\n";
    std::size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = send(sock, data.data() + sent, data.size() - sent, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

enum class ReadResult { Line, Closed, TooLong, Error };
ReadResult receiveLine(int sock, std::string& pending, std::string& line) {
    for (;;) {
        auto pos = pending.find('\n');
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
        if (n > 0) pending.append(buffer, static_cast<std::size_t>(n));
        else if (n == 0) return ReadResult::Closed;
        else if (errno != EINTR) return ReadResult::Error;
    }
}

// A small UI convenience. The server's actual protocol is still JOIN/SAY/QUIT.
std::string toRequest(const std::string& input) {
    if (input == "/quit") return "QUIT";
    if (input == "/users") return "USERS";    // Task 2: server not implemented.
    if (input == "/history") return "HISTORY"; // Task 3: server not implemented.
    if (input.rfind("/join ", 0) == 0) return "JOIN " + input.substr(6);
    if (input.rfind("/say ", 0) == 0) return "SAY " + input.substr(5);
    if (input == "QUIT" || input == "JOIN" || input == "SAY" ||
        input.rfind("JOIN ", 0) == 0 || input.rfind("SAY ", 0) == 0)
        return input;
    if (!input.empty() && input[0] == '/')
        return input.substr(1); // Extension point: /users, /history, etc.
    return "SAY " + input; // Plain words are chat messages.
}

std::mutex screenMutex; // Protect output from this client's own two threads.

void showServerLine(const std::string& line, bool interactive) {
    std::lock_guard<std::mutex> guard(screenMutex);
    if (interactive) std::cout << '\n';
    if (line.rfind("MSG ", 0) == 0) {
        const auto pos = line.find(' ', 4);
        if (pos != std::string::npos)
            std::cout << line.substr(4, pos - 4) << ": " << line.substr(pos + 1);
        else std::cout << line;
    } else if (line.rfind("INFO JOIN ", 0) == 0) {
        std::cout << "* " << line.substr(10) << " joined";
    } else if (line.rfind("INFO LEAVE ", 0) == 0) {
        std::cout << "* " << line.substr(11) << " left";
    } else if (line.rfind("OK JOIN ", 0) == 0) {
        std::cout << "* Joined as " << line.substr(8);
    } else if (line == "BYE") {
        std::cout << "* Goodbye";
    } else {
        std::cout << line;
    }
    std::cout << '\n';
    if (interactive && line != "BYE") std::cout << "> " << std::flush;
}

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    const int sock = connectToServer();
    const bool interactive = isatty(STDIN_FILENO) != 0;
    std::cout << "Connected to 127.0.0.1:" << PORT << "\n";
    std::cout << "Start with JOIN hana (or /join hana). Type messages or SAY text.\n";
    std::cout << "Type QUIT (or /quit) to leave.\n";

    std::atomic<bool> disconnected{false};
    // Receive thread: reads even when the main thread is waiting for keyboard input.
    std::thread receiver([&] {
        std::string pending;
        for (;;) {
            std::string line;
            auto r = receiveLine(sock, pending, line);
            if (r == ReadResult::Line) {
                showServerLine(line, interactive);
                if (line == "BYE") break;
            } else {
                if (r == ReadResult::TooLong)
                    showServerLine("ERR RESPONSE_TOO_LONG", interactive);
                else if (r == ReadResult::Error)
                    showServerLine("* Receive error", interactive);
                break;
            }
        }
        disconnected = true;
    });

    std::string input;
    for (;;) {
        if (disconnected) break;
        if (interactive) {
            std::lock_guard<std::mutex> guard(screenMutex);
            std::cout << "> " << std::flush;
        }
        if (!std::getline(std::cin, input)) {
            // Treat end-of-input like a normal QUIT so the server can reply BYE.
            if (!disconnected) sendLine(sock, "QUIT");
            break;
        }
        if (disconnected) break;
        if (input.empty()) continue;
        if (input.size() > MAX_INPUT_BYTES) {
            std::lock_guard<std::mutex> guard(screenMutex);
            std::cout << "Input too long (max 1024 bytes).\n";
            continue;
        }
        std::string request = toRequest(input);
        // The protocol limit applies to the complete command, not only the typed text.
        if (request.size() > MAX_LINE_BYTES) {
            std::lock_guard<std::mutex> guard(screenMutex);
            std::cout << "Request too long (max 256 bytes).\\n";
            continue;
        }
        if (!sendLine(sock, request)) {
            std::lock_guard<std::mutex> guard(screenMutex);
            std::cout << "Send failed.\n";
            break;
        }
        if (request == "QUIT") break;
    }
    // EOF or QUIT: allow incoming BYE/notifications until server closes.
    shutdown(sock, SHUT_WR);
    receiver.join();
    close(sock);
    return 0;
}
