#include <iostream>
#include "ThreadPool.h"

int main() {
    // 创建一个拥有 4 个工作线程的线程池
    ThreadPool pool(4);

    auto result1 = pool.enqueue([](int answer) {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟耗时操作
        return answer;
        }, 42);

    auto result2 = pool.enqueue([](int a, int b) {
        return a + b;
        }, 10, 20);

    std::cout << "Result 1: " << result1.get() << std::endl;
    std::cout << "Result 2: " << result2.get() << std::endl;

    std::vector<std::future<int>> results;
    for (int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.enqueue([i] {
                std::cout << "Task " << i << " is running on thread "
                    << std::this_thread::get_id() << std::endl;
                return i * i;
                })
        );
    }

    for (auto&& res : results) {
        std::cout << "Batch result: " << res.get() << std::endl;
    }

    return 0;
}