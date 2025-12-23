#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <cstring>

using namespace std;
using namespace chrono;

const int NUM_THREADS = 5;
const int NUM_ITERATIONS = 100000;

// 1. Мьютекс
mutex mtx;
void mutex_worker(int id, vector<char>& data) {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        lock_guard<mutex> lock(mtx);
        data[id] = static_cast<char>(33 + rand() % 94); 
    }
}

// 2. Семафор
class Semaphore {
private:
    mutex mtx;
    condition_variable cv;
    int count;
public:
    Semaphore(int count = 1) : count(count) {}
    
    void acquire() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this]() { return count > 0; });
        --count;
    }
    
    void release() {
        lock_guard<mutex> lock(mtx);
        ++count;
        cv.notify_one();
    }
};

Semaphore semaphore(1);
void semaphore_worker(int id, vector<char>& data) {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        semaphore.acquire();
        data[id] = static_cast<char>(33 + rand() % 94);
        semaphore.release();
    }
}

// 3. Барьер
class Barrier {
private:
    mutex mtx;
    condition_variable cv;
    int count;
    int generation;
    const int total;
public:
    Barrier(int total) : count(total), generation(0), total(total) {}
    
    void arrive_and_wait() {
        unique_lock<mutex> lock(mtx);
        int gen = generation;
        
        if (--count == 0) {
            ++generation;
            count = total;
            cv.notify_all();
        } else {
            cv.wait(lock, [this, gen]() { return gen != generation; });
        }
    }
};

// 4. SpinLock
atomic_flag spinlock = ATOMIC_FLAG_INIT;
void spinlock_worker(int id, vector<char>& data) {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        while (spinlock.test_and_set(memory_order_acquire)) {}
        data[id] = static_cast<char>(33 + rand() % 94);
        spinlock.clear(memory_order_release);
    }
}

// 5. SpinWait
atomic_flag spinlock2 = ATOMIC_FLAG_INIT;
void spinwait_worker(int id, vector<char>& data) {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        while (spinlock2.test_and_set(memory_order_acquire)) {
            this_thread::yield();
        }
        data[id] = static_cast<char>(33 + rand() % 94);
        spinlock2.clear(memory_order_release);
    }
}

// 6. Monitor (исправленная версия)
class Monitor {
private:
    mutex mtx;
    condition_variable cv;
    bool available;
public:
    Monitor() : available(true) {}
    
    void enter() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this](){ return available; });
        available = false;
    }
    
    void exit() {
        {
            lock_guard<mutex> lock(mtx);
            available = true;
        }
        cv.notify_one();
    }
};

Monitor monitor;
void monitor_worker(int id, vector<char>& data) {
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        monitor.enter();
        data[id] = static_cast<char>(33 + rand() % 94);
        monitor.exit();
    }
}

// Функция для запуска теста
void run_test(const string& name, void (*worker)(int, vector<char>&)) {
    vector<thread> threads;
    vector<char> data(NUM_THREADS, ' ');
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i, ref(data));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    
    cout << name << ": " << duration.count() << " ms" << endl;
}

// ДЕМОНСТРАЦИОННАЯ ФУНКЦИЯ: Запуск "гонки" с выводом символов
void race_demonstration() {
    cout << "\n=== ДЕМОНСТРАЦИЯ ГОНКИ ПОТОКОВ (Monitor) ===" << endl;
    cout << "Каждый поток выводит по 10 символов:" << endl;
    
    Monitor race_monitor;
    vector<thread> race_threads;
    atomic<int> counter{0};
    const int chars_per_thread = 10;
    
    auto race_worker = [&race_monitor, &counter](int id) {
        for (int i = 0; i < chars_per_thread; ++i) {
            race_monitor.enter();
            cout << "Поток " << id << ": '" 
                 << static_cast<char>(33 + rand() % 94) 
                 << "' (шаг " << counter++ << ")" << endl;
            race_monitor.exit();
            this_thread::sleep_for(milliseconds(10)); // Замедляем для наглядности
        }
    };
    
    for (int i = 0; i < NUM_THREADS; ++i) {
        race_threads.emplace_back(race_worker, i);
    }
    
    for (auto& t : race_threads) {
        t.join();
    }
}

int main() {
    srand(time(nullptr));
    
    cout << "Сравнение примитивов синхронизации (" << NUM_THREADS << " потоков, " 
         << NUM_ITERATIONS << " итераций):" << endl;
    
    run_test("Mutex        ", mutex_worker);
    run_test("Semaphore    ", semaphore_worker);
    
    // Для barrier нужно отдельное создание в каждом тесте
    {
        Barrier barrier(NUM_THREADS);
        auto barrier_wrapper = [&barrier](int id, vector<char>& data) {
            for (int i = 0; i < NUM_ITERATIONS; ++i) {
                data[id] = static_cast<char>(33 + rand() % 94);
                barrier.arrive_and_wait();
            }
        };
        
        vector<thread> threads;
        vector<char> data(NUM_THREADS, ' ');
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(barrier_wrapper, i, ref(data));
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        cout << "Barrier      : " << duration.count() << " ms" << endl;
    }
    
    run_test("SpinLock     ", spinlock_worker);
    run_test("SpinWait     ", spinwait_worker);
    run_test("Monitor      ", monitor_worker);
    
    // Запускаем демонстрацию гонки
    race_demonstration();
    
    return 0;
}
