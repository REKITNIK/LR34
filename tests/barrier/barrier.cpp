// Выполняют функцию остановки определённого количества потоков до тех пор, пока они все не завершат выполнение операции. То есть один поток в определённой точке должен будет ждать других.

#include <barrier>
#include <thread>
#include <vector>
#include <iostream>

int main() {
    auto on_completion = []() noexcept { std::cout << "Фаза завершена\\n"; };
    // Создаем барьер для 3 потоков
    std::barrier sync_point(3, on_completion);

    auto worker = [&]() {
        // Потоки работают...
        sync_point.arrive_and_wait(); // Ждут друг друга
        // Потоки продолжают...
    };

    std::vector<std::thread> threads;
    for(int i=0; i<3; ++i) threads.emplace_back(worker);
    for(auto& t : threads) t.join();
}

