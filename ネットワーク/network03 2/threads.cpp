// 待機を重ねる比較。計算処理が2倍速くなる実験ではない。
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
std::mutex outputMutex;
void run(const std::string& name) {
    for (int i = 0; i < 3; ++i) {
        { // 表示1行だけを保護する。sleepはロックの外。
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << name << ": count=" << i << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
int main(int argc, char* argv[]) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) != "serial")) {
        std::cerr << "Usage: ./threads [serial]\n";
        return 1;
    }
    const auto start = std::chrono::steady_clock::now();
    if (argc == 2) {
        run("A");
        run("B");
    } else {
        std::thread a(run, "A");
        std::thread b;
        try { b = std::thread(run, "B"); }
        catch (...) { a.join(); throw; }
        a.join();
        b.join();
    }
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    std::cout << "Elapsed: " << elapsed.count() << " s\n";
}
