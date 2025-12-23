#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <random>
#include <atomic>
#include <condition_variable>
#include <shared_mutex>

class Philosopher {
private:
    int id;
    std::timed_mutex& leftFork;  // timed_mutex для try_lock_for, обычный mutex в себе такого метода не хранит
    std::timed_mutex& rightFork; 
    std::mutex& printMutex;
    std::atomic<bool>& stopFlag;
    int mealsEaten;
    
    // Для случайной генерации времени
    std::mt19937 generator;
    std::uniform_int_distribution<int> thinkDist;
    std::uniform_int_distribution<int> eatDist;
    
    void think() {
        int thinkTime = thinkDist(generator);
        {
            std::lock_guard<std::mutex> lock(printMutex);
            std::cout << "Философ " << id << " размышляет " << thinkTime << " мс" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(thinkTime));
    }
    
    void eat() {
        int eatTime = eatDist(generator);
        {
            std::lock_guard<std::mutex> lock(printMutex);
            std::cout << "Философ " << id << " ест " << eatTime << " мс (всего съел: " << mealsEaten + 1 << " раз)" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(eatTime));
        mealsEaten++;
    }
    
public:
    Philosopher(int id, std::timed_mutex& left, std::timed_mutex& right, std::mutex& print, std::atomic<bool>& stop)
        : id(id), leftFork(left), rightFork(right), printMutex(print), 
          stopFlag(stop), mealsEaten(0), generator(std::random_device{}()),
          thinkDist(100, 500), eatDist(200, 400) {}
    
    // Версия 1: Простая, но может привести к deadlock
    void dineWithDeadlockRisk() {
        while (!stopFlag) {
            think();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " пытается взять левую вилку" << std::endl;
            }
            leftFork.lock();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " взял левую вилку, пытается взять правую" << std::endl;
            }
            rightFork.lock();
            
            eat();
            
            rightFork.unlock();
            leftFork.unlock();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " положил вилки" << std::endl;
            }
        }
    }
    
    // Версия 2: С использованием std::lock() для избежания deadlock
    void dineWithStdLock() {
        while (!stopFlag) {
            think();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " пытается взять вилки (безопасно)" << std::endl;
            }
            
            // std::lock блокирует оба мьютекса атомарно
            std::lock(leftFork, rightFork);
            std::lock_guard<std::timed_mutex> lockLeft(leftFork, std::adopt_lock);
            std::lock_guard<std::timed_mutex> lockRight(rightFork, std::adopt_lock);
            
            eat();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " положил вилки" << std::endl;
            }
        }
    }
    
    // Версия 3: С использованием таймаутов
    void dineWithTimeout() {
        while (!stopFlag) {
            think();
            
            bool gotForks = false;
            int attempts = 0;
            
            while (!gotForks && !stopFlag && attempts < 3) {
                {
                    std::lock_guard<std::mutex> lock(printMutex);
                    std::cout << "Философ " << id << " пытается взять вилки (попытка " 
                              << attempts + 1 << ")" << std::endl;
                }
                
                // Пытаемся взять левую вилку с таймаутом
                if (leftFork.try_lock_for(std::chrono::milliseconds(100))) {
                    // Пытаемся взять правую вилку с таймаутом
                    if (rightFork.try_lock_for(std::chrono::milliseconds(100))) {
                        gotForks = true;
                    } else {
                        // Не смогли взять правую вилку - отпускаем левую
                        leftFork.unlock();
                    }
                }
                
                if (!gotForks) {
                    attempts++;
                    if (attempts < 3) {
                        {
                            std::lock_guard<std::mutex> lock(printMutex);
                            std::cout << "Философ " << id << " не смог взять вилки, ждет" << std::endl;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }
            
            if (gotForks) {
                eat();
                
                rightFork.unlock();
                leftFork.unlock();
                
                {
                    std::lock_guard<std::mutex> lock(printMutex);
                    std::cout << "Философ " << id << " положил вилки" << std::endl;
                }
            } else {
                {
                    std::lock_guard<std::mutex> lock(printMutex);
                    std::cout << "Философ " << id << " голодает :(" << std::endl;
                }
            }
        }
    }
    
    // Версия 4: Только один философ может брать вилки за раз (семафор)
    void dineWithSemaphore(std::mutex& tableMutex) {
        while (!stopFlag) {
            think();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " ждет разрешения сесть за стол" << std::endl;
            }
            
            // Блокируем доступ к столу
            tableMutex.lock();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " взял вилки" << std::endl;
            }
            
            leftFork.lock();
            rightFork.lock();
            
            eat();
            
            rightFork.unlock();
            leftFork.unlock();
            tableMutex.unlock();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " положил вилки и освободил стол" << std::endl;
            }
        }
    }
    
    // Версия 5: Философ с четным ID берет сначала левую вилку, с нечетным - правую
    void dineWithOrdering() {
        while (!stopFlag) {
            think();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " берет вилки в определенном порядке" << std::endl;
            }
            
            if (id % 2 == 0) {
                // Четные философы: левая, затем правая
                leftFork.lock();
                rightFork.lock();
            } else {
                // Нечетные философы: правая, затем левая
                rightFork.lock();
                leftFork.lock();
            }
            
            eat();
            
            // Освобождаем в обратном порядке
            if (id % 2 == 0) {
                rightFork.unlock();
                leftFork.unlock();
            } else {
                leftFork.unlock();
                rightFork.unlock();
            }
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " положил вилки" << std::endl;
            }
        }
    }
    
    // Версия 6: Используем condition_variable для координации
    void dineWithConditionVariable(std::condition_variable& cv, std::mutex& cv_mutex, std::atomic<int>& eatingCount, int maxEating) {
        while (!stopFlag) {
            think();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " хочет есть" << std::endl;
            }
            
            // Ждем, пока не будет места за столом
            {
                std::unique_lock<std::mutex> lock(cv_mutex);
                cv.wait(lock, [&]() { return eatingCount < maxEating; });
                eatingCount++;
            }
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " начал брать вилки (сейчас ест: " 
                          << eatingCount << " философов)" << std::endl;
            }
            
            // Берем вилки
            std::lock(leftFork, rightFork);
            std::lock_guard<std::timed_mutex> lockLeft(leftFork, std::adopt_lock);
            std::lock_guard<std::timed_mutex> lockRight(rightFork, std::adopt_lock);
            
            eat();
            
            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << "Философ " << id << " закончил есть" << std::endl;
            }
            
            // Освобождаем место за столом
            {
                std::lock_guard<std::mutex> lock(cv_mutex);
                eatingCount--;
                cv.notify_all();
            }
        }
    }
    
    int getMealsEaten() const {
        return mealsEaten;
    }
};

// Функция для запуска теста
void runPhilosophersTest(int version, int durationSeconds) {
    const int NUM_PHILOSOPHERS = 5;
    
    std::vector<std::timed_mutex> forks(NUM_PHILOSOPHERS);  // Изменено на timed_mutex
    std::mutex printMutex;
    std::mutex tableMutex; // для версии 4
    std::atomic<bool> stopFlag(false);
    
    // Для версии 6
    std::condition_variable cv;
    std::mutex cv_mutex;
    std::atomic<int> eatingCount(0);
    const int MAX_EATING = 2; // Максимум 2 философа могут есть одновременно
    
    std::vector<Philosopher> philosophers;
    std::vector<std::thread> threads;
    
    // Создаем философов
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers.emplace_back(i, forks[i], forks[(i + 1) % NUM_PHILOSOPHERS], 
                                  printMutex, stopFlag);
    }
    
    std::cout << "\n=== Запуск теста версии " << version << " (длительность: " 
              << durationSeconds << " сек) ===" << std::endl;
    
    // Запускаем потоки в зависимости от версии
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        switch (version) {
            case 1:
                threads.emplace_back(&Philosopher::dineWithDeadlockRisk, &philosophers[i]);
                break;
            case 2:
                threads.emplace_back(&Philosopher::dineWithStdLock, &philosophers[i]);
                break;
            case 3:
                threads.emplace_back(&Philosopher::dineWithTimeout, &philosophers[i]);
                break;
            case 4:
                threads.emplace_back(&Philosopher::dineWithSemaphore, &philosophers[i], std::ref(tableMutex));
                break;
            case 5:
                threads.emplace_back(&Philosopher::dineWithOrdering, &philosophers[i]);
                break;
            case 6:
                threads.emplace_back(&Philosopher::dineWithConditionVariable, &philosophers[i], 
                                     std::ref(cv), std::ref(cv_mutex), std::ref(eatingCount), MAX_EATING);
                break;
        }
    }
    
    // Ждем указанное время
    std::this_thread::sleep_for(std::chrono::seconds(durationSeconds));
    
    // Останавливаем философов
    stopFlag = true;
    
    // Ждем завершения всех потоков
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Выводим статистику
    std::cout << "\n=== Статистика версии " << version << " ===" << std::endl;
    int totalMeals = 0;
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        std::cout << "Философ " << i << " поел " << philosophers[i].getMealsEaten() << " раз" << std::endl;
        totalMeals += philosophers[i].getMealsEaten();
    }
    std::cout << "Всего съедено: " << totalMeals << " раз" << std::endl;
    if (NUM_PHILOSOPHERS > 0) {
        std::cout << "Среднее на философа: " << totalMeals / NUM_PHILOSOPHERS << std::endl;
    }
}

int main() {
    std::cout << "=================================================================" << std::endl;
    std::cout << "            ПРОБЛЕМА ОБЕДАЮЩИХ ФИЛОСОФОВ" << std::endl;
    std::cout << "=================================================================" << std::endl;
    std::cout << "Описание: 5 философов, 5 вилок, спагетти едят двумя вилками" << std::endl;
    std::cout << "=================================================================\n" << std::endl;
    
    // Запускаем разные версии решения
    std::cout << "Версия 1: Риск взаимной блокировки (deadlock)" << std::endl;
    std::cout << "Версия 2: Безопасная блокировка с std::lock" << std::endl;
    std::cout << "Версия 3: С таймаутами на взятие вилок" << std::endl;
    std::cout << "Версия 4: Семафор (только один философ за столом)" << std::endl;
    std::cout << "Версия 5: Упорядоченный захват вилок (четные/нечетные)" << std::endl;
    std::cout << "Версия 6: Condition variable (макс 2 философа одновременно)" << std::endl;
    std::cout << "=================================================================\n" << std::endl;
    
    int testDuration = 5; // секунд (уменьшил для быстрого теста)
    
    // Тестируем все версии
    for (int version = 1; version <= 6; version++) {
        runPhilosophersTest(version, testDuration);
    }
    
    std::cout << "\n=================================================================" << std::endl;
    std::cout << "           ТЕСТИРОВАНИЕ ЗАВЕРШЕНО" << std::endl;
    std::cout << "=================================================================" << std::endl;
    
    // Демонстрация deadlock в версии 1 (если повезет)
    std::cout << "\n=== Демонстрация deadlock (версия 1) ===" << std::endl;
    std::cout << "Запускаем на 3 секунды, возможно возникнет deadlock..." << std::endl;
    
    const int NUM_PHILOSOPHERS = 5;
    std::vector<std::timed_mutex> forks(NUM_PHILOSOPHERS);
    std::mutex printMutex;
    std::atomic<bool> stopFlag(false);
    
    std::vector<Philosopher> philosophers;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        philosophers.emplace_back(i, forks[i], forks[(i + 1) % NUM_PHILOSOPHERS], 
                                  printMutex, stopFlag);
    }
    
    // Запускаем все потоки одновременно
    for (int i = 0; i < NUM_PHILOSOPHERS; ++i) {
        threads.emplace_back(&Philosopher::dineWithDeadlockRisk, &philosophers[i]);
    }
    
    // Даем поработать 3 секунды
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // Проверяем, не зависли ли философы
    std::cout << "Проверяем активность философов..." << std::endl;
    
    // Останавливаем
    stopFlag = true;
    
    // Даем еще 2 секунды на завершение
    bool allFinished = true;
    for (int i = 0; i < 20; ++i) {
        allFinished = true;
        for (auto& thread : threads) {
            if (thread.joinable()) {
                allFinished = false;
                break;
            }
        }
        if (allFinished) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!allFinished) {
        std::cout << "\n!!! ОБНАРУЖЕН DEADLOCK !!!" << std::endl;
        std::cout << "Философы зависли в вечном ожидании." << std::endl;
        std::cout << "Это классический пример взаимной блокировки." << std::endl;
        
        // Принудительно завершаем программу
        std::cout << "Принудительное завершение программы..." << std::endl;
        std::exit(1);
    } else {
        std::cout << "Deadlock не обнаружен в этот раз (повезло!)." << std::endl;
        
        // Ждем нормального завершения
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    std::cout << "\n=================================================================" << std::endl;
    std::cout << "           ВЫВОДЫ И РЕКОМЕНДАЦИИ" << std::endl;
    std::cout << "=================================================================" << std::endl;
    std::cout << "1. Версия 1 (простая) - риск deadlock, не использовать в production!" << std::endl;
    std::cout << "2. Версия 2 (std::lock) - безопасная, хорошая производительность" << std::endl;
    std::cout << "3. Версия 3 (таймауты) - устойчивая к голоданию, но сложная" << std::endl;
    std::cout << "4. Версия 4 (семафор) - безопасная, но низкая производительность" << std::endl;
    std::cout << "5. Версия 5 (упорядочение) - простая и эффективная" << std::endl;
    std::cout << "6. Версия 6 (condition variable) - гибкая, можно регулировать нагрузку" << std::endl;
    std::cout << "=================================================================" << std::endl;
    
    return 0;
}
