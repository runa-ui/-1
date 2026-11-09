// 第5回：TCPの受信が任意の位置で分割・結合しても、行を取り出せる考え方。
// ネットワーク通信は行わず、recvで届き得る「かたまり」を模擬する。
#include <iostream>
#include <string>
#include <vector>

int main() {
    // 3回に分かれて届いたと仮定（1つの塊に2行分が含まれることもある）。
    const std::vector<std::string> chunks = {
        "JOIN ha",
        "na\nSAY hel",
        "lo\nQUIT\n"
    };

    std::string pending;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        std::cout << "chunk " << (i + 1) << " arrived\n";
        pending += chunks[i]; // 受信済みだが未処理のデータに追加

        while (true) {
            std::size_t pos = pending.find('\n');
            if (pos == std::string::npos) break; // 改行がまだない
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);          // 残りを保持
            std::cout << "  one line: [" << line << "]\n";
        }
    }
}
