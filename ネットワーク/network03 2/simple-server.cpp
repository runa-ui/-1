// 第3回 基本版：1回受信し、受け取った分だけ返す。macOS / Linux / WSL用。
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

void fail(const char* operation) {
    std::perror(operation);
    std::exit(EXIT_FAILURE); // プロセス終了時にOSがソケットを解放する。
}

// 待受用ソケットの準備
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

// 接続した相手と通信するためのソケットを受け取る。
int acceptClientConnection(int server_fd) {
    int client_socket = accept(server_fd, nullptr, nullptr);
    if (client_socket < 0) fail("accept");
    return client_socket;
}

ssize_t receiveMessage(int sock, char* buffer, int size) {
    return recv(sock, buffer, size, 0); // 1回だけ受信。全文とは限らない。
}

void sendMessage(int sock, const char* data, std::size_t size) {
    ssize_t sent = send(sock, data, size, 0);
    if (sent < 0) fail("send");
    if (static_cast<std::size_t>(sent) != size) {
        std::cerr << "Partial send: retrying the rest is not implemented.\n";
        std::exit(EXIT_FAILURE); // 残りを送る処理は発展版で扱う。
    }
}

int main() {
    std::signal(SIGPIPE, SIG_IGN); // 切断後の送信エラーは戻り値で扱う。
    int server_fd = createServerSocket();
    std::cout << "Waiting on 127.0.0.1:" << PORT << std::endl;
    int client_socket = acceptClientConnection(server_fd);
    std::cout << "Client connected." << std::endl;

    char buffer[BUFFER_SIZE];
    ssize_t received = receiveMessage(client_socket, buffer, BUFFER_SIZE);
    if (received < 0) fail("recv");
    if (received > 0) {
        std::cout << "Received " << received << " bytes: ";
        std::cout.write(buffer, received); // 終端の0ではなく受信長を使う。
        std::cout << std::endl;
        sendMessage(client_socket, buffer, received);
    } else {
        std::cout << "Peer finished without data." << std::endl;
    }
    close(client_socket);
    close(server_fd);
}
