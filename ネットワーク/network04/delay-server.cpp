// 第4回：第3回のサーバに「応答遅延・連続受付」を追加。macOS / Linux / WSL用。
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
// 第4回追加：応答を遅らせ、起動時の引数を指定する。
#include <chrono>
#include <thread>
#include <string>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

void fail(const char* operation) {
    std::perror(operation);
    std::exit(EXIT_FAILURE); // プロセス終了時にOSがソケットを解放する。
}

// 待受用ソケットの準備（第3回と同じ）
int createServerSocket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) fail("socket");
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        fail("setsockopt");

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 同じPCから接続
    address.sin_port = htons(PORT);
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
        fail("bind");
    if (listen(server_fd, 3) < 0) fail("listen");
    return server_fd;
}

// 接続した相手と通信するためのソケットを受け取る（第3回と同じ）。
int acceptClientConnection(int server_fd) {
    int client_socket = accept(server_fd, nullptr, nullptr);
    if (client_socket < 0) fail("accept");
    return client_socket;
}

ssize_t receiveMessage(int sock, char* buffer, int size) {
    return recv(sock, buffer, size, 0); // 1回だけ受信。全文とは限らない。
}

// 第4回変更：切断された相手への送信に失敗しても、次の接続を受け付ける。
bool sendMessage(int sock, const char* data, std::size_t size) {
    ssize_t sent = send(sock, data, size, 0);
    if (sent < 0) {
        std::perror("server send");
        return false;
    }
    if (static_cast<std::size_t>(sent) != size) {
        std::cerr << "Partial send (teaching limit).\n";
        return false; // 残りを送る処理は今回も省略。
    }
    return true;
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
    // 第4回追加：応答前の待ち時間と、返答するかどうかを選ぶ。
    if (argc > 3) {
        std::cerr << "Usage: ./delay-server [delay_ms] [reply|close]\n";
        return 1;
    }
    const int delay_ms = argc > 1 ? number(argv[1], 10000) : 0;
    const std::string mode = argc > 2 ? argv[2] : "reply";
    if (mode != "reply" && mode != "close") {
        std::cerr << "Use reply or close.\n";
        return 1;
    }

    std::signal(SIGPIPE, SIG_IGN); // 切断後の送信エラーは戻り値で扱う。
    int server_fd = createServerSocket();
    std::cout << "Waiting on 127.0.0.1:" << PORT
              << " delay_ms=" << delay_ms << " mode=" << mode << std::endl;

    int processed = 0; // 第4回追加：処理した要求の回数。再起動で0に戻る。
    while (true) {     // 第4回追加：1回の通信が終わっても次を待ち受ける。
        int client_socket = acceptClientConnection(server_fd);
        std::cout << "Client connected." << std::endl;

        char buffer[BUFFER_SIZE];
        ssize_t received = receiveMessage(client_socket, buffer, BUFFER_SIZE);
        if (received < 0) {
            std::perror("server recv"); // エラーがあっても次の接続を待つ。
        } else if (received > 0) {
            ++processed; // 第4回追加：返答する前に「処理済み」にする。
            std::cout << "Processed #" << processed << " bytes=" << received
                      << " message=";
            std::cout.write(buffer, received);
            std::cout << std::endl;

            // 第4回追加：遅れて返す（待っている間は次の接続を処理しない）。
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            if (mode == "reply") {
                if (sendMessage(client_socket, buffer, received))
                    std::cout << "Reply handed to OS." << std::endl;
            } else {
                std::cout << "Closed without reply." << std::endl;
            }
        } else {
            std::cout << "Peer finished without data." << std::endl;
        }
        close(client_socket); // 接続用だけ閉じ、待受用server_fdは残す。
    }
}