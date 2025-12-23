// Синхронизирует потоки, чтобы они шли в ногу при выполнении операций

#include <mutex>

class Monitor {
private:
    int shared_resource; // Разделяемый ресурс
    std::mutex mtx;     // Мьютекс для защиты
public:
    void safe_update(int new_value) {
        std::lock_guard<std::mutex> lock(mtx); // Автоматическая блокировка
        shared_resource = new_value;
    }
};

