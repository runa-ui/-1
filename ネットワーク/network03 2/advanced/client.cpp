// macOS / Linux / WSL。同じ環境内のサーバに1行送って返答を受ける。
#include "net_common.hpp"
constexpr unsigned short PORT = 8080;
constexpr std::size_t BUFFER_SIZE = 1024; // 観察では4に変更する。

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    try {
        Socket sock(socket(AF_INET, SOCK_STREAM, 0));
        auto address = loopbackAddress(PORT);
        if (connect(sock.fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            fail("connect");

        std::cout << "Enter message: " << std::flush;
        std::string message;
        if (!std::getline(std::cin, message))
            throw std::runtime_error("input ended before a line was read");
        if (message.size() > MAX_BYTES)
            throw std::runtime_error("message exceeds 64 KiB");
        sendAll(sock.fd, message);
        // 送信側だけを終了。相手のrecvが0になり、引き続き応答を受信できる。
        if (shutdown(sock.fd, SHUT_WR) < 0) fail("shutdown");

        const auto reply = receiveUntilEof(sock.fd, BUFFER_SIZE, true);
        std::cout << "Echo " << reply.size() << " bytes: " << reply << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
