// Поток крутится в цикле, пока не будет снята блокировка на продолжение. spinwait - то же самое, только поток ждёи не снятия блокировки, а конкретного условия

#include <atomic>

class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        // Активное ожидание: крутимся, пока flag установлен
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Можно добавить pause для снижения нагрузки
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

