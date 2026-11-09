// macOS / Linux / WSL。1接続・1往復で終了するTCPエコーサーバ。
#include "net_common.hpp"
constexpr unsigned short PORT = 8080;
constexpr std::size_t BUFFER_SIZE = 1024;

int main() {
    // 切断済みの相手へのsendは、戻り値のエラーとして扱う。
    std::signal(SIGPIPE, SIG_IGN);
    try {
        Socket server(socket(AF_INET, SOCK_STREAM, 0));
        const int server_fd = server.fd;
        const int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            fail("setsockopt");
        auto address = loopbackAddress(PORT);
        if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            fail("bind");
        if (listen(server_fd, 3) < 0) fail("listen");
        std::cout << "Waiting on 127.0.0.1:" << PORT << std::endl;

        int accepted;
        do { accepted = accept(server_fd, nullptr, nullptr); }
        while (accepted < 0 && errno == EINTR);
        Socket client(accepted);
        const int client_fd = client.fd;
        std::cout << "Client connected." << std::endl;

        const auto message = receiveUntilEof(client_fd, BUFFER_SIZE, false);
        std::cout << "Received " << message.size() << " bytes: " << message << std::endl;
        sendAll(client_fd, message);
        // スコープ終了でclient/serverがcloseされ、相手は応答の終了を知る。
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
