// 第5回：行単位の通信仕様を試す、1対1のチャット・プロトコル確認用サーバ。
// macOS / Linux / WSL用。複数人への配信は第6回以降に扱います。
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

constexpr int PORT = 8081;
constexpr std::size_t MAX_LINE_BYTES = 256; // 改行を除く1行の上限
constexpr std::size_t MAX_NAME_BYTES = 16;
constexpr std::size_t MAX_TEXT_BYTES = 200;

void fail(const char* operation) {
    std::perror(operation);
    std::exit(EXIT_FAILURE);
}

int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) fail("socket");

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        fail("setsockopt");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 同じPCからのみ接続
    address.sin_port = htons(PORT);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        fail("bind");
    if (listen(server_fd, 3) < 0) fail("listen");
    return server_fd;
}

int acceptClientConnection(int server_fd) {
    int sock = accept(server_fd, nullptr, nullptr);
    if (sock < 0) fail("accept");
    return sock;
}

// 第5回追加：sendが一度で全バイトを送れない場合も、残りを送る。
bool sendMessage(int sock, const std::string& line) {
    const std::string data = line + "\n"; // 1行＝1メッセージ
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

// 第5回追加：recvの区切りではなく '\n' を探して1行を取り出す。
ReadResult receiveLine(int sock, std::string& pending, std::string& line) {
    while (true) {
        std::size_t pos = pending.find('\n');
        if (pos != std::string::npos) {
            if (pos > MAX_LINE_BYTES) return ReadResult::TooLong;
            line = pending.substr(0, pos);
            pending.erase(0, pos + 1); // 次の行はpendingに残す
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

// 名前は英数字とアンダースコアのみ（1～16バイト）。
bool validName(const std::string& name) {
    if (name.empty() || name.size() > MAX_NAME_BYTES) return false;
    for (unsigned char c : name) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// 名前登録前と登録後で、受け付ける命令を変える。
std::string processMessage(const std::string& line, std::string& name, bool& quit) {
    if (line == "QUIT") {
        quit = true;
        return "BYE";
    }
    if (line.empty()) return "ERR EMPTY";
    if (line == "JOIN" || line == "SAY" || line.rfind("QUIT ", 0) == 0)
        return "ERR BAD_FORMAT";

    if (line.rfind("JOIN ", 0) == 0) {
        if (!name.empty()) return "ERR ALREADY_JOINED";
        const std::string requested = line.substr(5);
        if (!validName(requested)) return "ERR BAD_NAME";
        name = requested;
        return "OK JOIN " + name;
    }
    if (line.rfind("SAY ", 0) == 0) {
        if (name.empty()) return "ERR NOT_JOINED";
        const std::string message = line.substr(4);
        if (message.empty()) return "ERR BAD_FORMAT";
        if (message.size() > MAX_TEXT_BYTES) return "ERR TEXT_TOO_LONG";
        for (unsigned char c : message) {
            if (c < 0x20 || c == 0x7f) return "ERR BAD_FORMAT";
        }
        return "MSG " + name + " " + message; // 今回は送信者本人に返すだけ
    }
    return "ERR UNKNOWN_COMMAND";
}

int main() {
    std::signal(SIGPIPE, SIG_IGN); // 切断した相手へのsendで終了しない
    int server_fd = createServerSocket();
    std::cout << "Waiting on 127.0.0.1:" << PORT << std::endl;

    while (true) { // 今回は1接続ずつ。複数接続は次回以降。
        int sock = acceptClientConnection(server_fd);
        std::cout << "Client connected." << std::endl;

        std::string pending;  // まだ1行になっていない受信データ
        std::string name;     // 空なら未登録
        while (true) {
            std::string line;
            ReadResult result = receiveLine(sock, pending, line);
            if (result == ReadResult::Closed || result == ReadResult::Error) break;
            if (result == ReadResult::TooLong) {
                sendMessage(sock, "ERR TOO_LONG");
                break; // 長すぎる行は接続を終了する
            }

            bool quit = false;
            const std::string reply = processMessage(line, name, quit);
            if (!sendMessage(sock, reply) || quit) break;
        }

        close(sock);
        std::cout << "Client disconnected." << std::endl;
    }
}
