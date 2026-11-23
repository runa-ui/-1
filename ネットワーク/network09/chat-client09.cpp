// Network Systems 09: [based on the unchanged lesson07 chat-client.cpp] chat client with an independent receive thread (C++17).
// JOIN name -> OK JOIN; SAY text -> MSG to all; QUIT -> BYE.
// Plain text automatically becomes SAY text; /quit is also accepted.
// macOS / Linux / WSL; local 127.0.0.1 server, PORT 8081.
#include <atomic>
#include <algorithm> // [09 ADD: AUTO]
#include <chrono>    // [09 ADD: AUTO/LOG]
#include <cmath>      // [09 ADD: AUTO]
#include <fstream>    // [09 ADD: LOG]
#include <iomanip>    // [09 ADD: LOG]
#include <vector>     // [09 ADD: AUTO]
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

constexpr int PORT = 8081; // Same port as lesson07
constexpr std::size_t MAX_LINE_BYTES = 256;
constexpr std::size_t MAX_INPUT_BYTES = 1024;

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
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) fail("inet_pton"); // [09 CHANGE]
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

// ===== [09 ADD: AUTO + LOG START] =====
// The original interactive mode below is unchanged. Auto mode uses the same
// connectToServer/sendLine/receiveLine functions from lesson07.
int runAuto(int argc, char* argv[]) {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: ./chat-client09 --auto <name> <1..200> <A|B> [hold_sec:0..10]\n";
        return 1;
    }
    const std::string name = argv[2];
    const std::string mode = argv[4];
    if (name.empty() || name.size() > 16 || (mode != "A" && mode != "B")) {
        std::cerr << "Invalid client name or mode.\n";
        return 1;
    }
    for (unsigned char c : name)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 1;
    int count = 0, hold = 0;
    try {
        std::size_t n1 = 0, n2 = 0;
        count = std::stoi(argv[3], &n1);
        if (n1 != std::string(argv[3]).size()) return 1;
        if (argc == 6) {
            hold = std::stoi(argv[5], &n2);
            if (n2 != std::string(argv[5]).size()) return 1;
        }
    } catch (...) { return 1; }
    if (count < 1 || count > 200 || hold < 0 || hold > 10) return 1;

    int sock = connectToServer();
    timeval recvTimeout{3, 0}; // Avoid waiting forever when server fails.
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               &recvTimeout, sizeof(recvTimeout));
    std::string pending, line;
    if (!sendLine(sock, "JOIN " + name)) {
        close(sock);
        return 2;
    }
    bool joined = false;
    while (receiveLine(sock, pending, line) == ReadResult::Line) {
        if (line == "OK JOIN " + name) { joined = true; break; }
        if (line.rfind("ERR ", 0) == 0) break;
    }
    if (!joined) {
        std::cerr << "JOIN failed for " << name << '\n';
        close(sock);
        return 2;
    }
    // A top/ps observation run can hold connections before the burst.
    if (hold > 0) std::this_thread::sleep_for(std::chrono::seconds(hold));

    // One CSV per client process avoids concurrent writes to the same file.
    std::ofstream log("client-log-" + name + ".csv", std::ios::app);
    if (!log) { std::cerr << "Cannot open client log\n"; close(sock); return 2; }
    if (log.tellp() == 0)
        log << "mode,client,seq,success,send_steady_ns,finish_steady_ns,rtt_ms\n";

    using Clock = std::chrono::steady_clock;
    int success = 0;
    double totalMs = 0.0;
    std::vector<double> values;
    for (int i = 0; i < count; ++i) {
        const std::string token = "trial_" + std::to_string(i);
        const std::string expected = "MSG " + name + " " + token;
        const auto start = Clock::now();
        bool found = sendLine(sock, "SAY " + token);
        if (found) {
            found = false;
            while (receiveLine(sock, pending, line) == ReadResult::Line) {
                if (line == expected) { found = true; break; }
                if (line.rfind("ERR ", 0) == 0) break;
                // Other users' MSG/INFO are not the response to this SAY.
            }
        }
        const auto finish = Clock::now();
        const auto sendNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            start.time_since_epoch()).count();
        const auto finishNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            finish.time_since_epoch()).count();
        const double rtt = std::chrono::duration<double,std::milli>(
            finish-start).count();
        log << mode << ',' << name << ',' << i << ',' << (found ? 1 : 0)
            << ',' << sendNs << ',' << finishNs << ','
            << std::fixed << std::setprecision(3) << rtt << '\n';
        if (!found) break; // Stop on a failed request; remaining are missing.
        ++success;
        totalMs += rtt;
        values.push_back(rtt);
    }
    sendLine(sock, "QUIT");
    shutdown(sock, SHUT_WR);
    close(sock);
    std::sort(values.begin(), values.end());
    double avg = success ? totalMs / success : 0;
    double p95 = success ? values[static_cast<std::size_t>(
        std::ceil(success*0.95))-1] : 0;
    std::cout << mode << ' ' << name << " success=" << success
              << "/" << count << " avg_ms=" << std::fixed
              << std::setprecision(2) << avg << " p95_ms=" << p95 << '\n';
    return success == count ? 0 : 2;
}
// ===== [09 ADD: AUTO + LOG END] =====

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);
    // [09 ADD: AUTO] Only --auto takes the new path; normal chat stays intact.
    if (argc > 1) {
        if (std::string(argv[1]) != "--auto") {
            std::cerr << "Usage: ./chat-client09 [--auto <name> <count> <A|B> [hold_sec]]\n";
            return 1;
        }
        return runAuto(argc, argv);
    }
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
        if (!std::getline(std::cin, input)) break;
        if (disconnected) break;
        if (input.empty()) continue;
        if (input.size() > MAX_INPUT_BYTES) {
            std::lock_guard<std::mutex> guard(screenMutex);
            std::cout << "Input too long (max 1024 bytes).\n";
            continue;
        }
        std::string request = toRequest(input);
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
