#pragma once

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <random>
#include <string>
#include <semaphore>
#include <barrier>
#include <functional>
#include "benchmark.h"

class ThreadRaceTest {
private:
    std::vector<std::thread> threads;
    std::vector<char> results;
    std::vector<long long> threadTimes;
    int numThreads;
    int raceLength;
    
    // Случайная генерация символов
    char generateRandomChar() {
        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(33, 126); // Печатные ASCII символы
        return static_cast<char>(distribution(generator));
    }
    
public:
    ThreadRaceTest(int threadsCount, int length = 1000) 
        : numThreads(threadsCount), raceLength(length) {
        results.resize(numThreads, ' ');
        threadTimes.resize(numThreads, 0);
    }
    
    // Тест с использованием мьютекса
    void testWithMutex() {
        std::mutex mtx;
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &mtx]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    std::lock_guard<std::mutex> lock(mtx);
                    results[i] = generateRandomChar();
                    // Имитация работы
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "Mutex Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Тест с использованием семафора 
    void testWithSemaphore() {
        std::counting_semaphore<1000> sem(1);
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &sem]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    sem.acquire();
                    results[i] = generateRandomChar();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    sem.release();
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "Semaphore Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Тест с использованием барьера 
    void testWithBarrier() {
        std::barrier syncPoint(numThreads);
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &syncPoint]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    results[i] = generateRandomChar();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    syncPoint.arrive_and_wait();
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "Barrier Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Тест со SpinLock
    void testWithSpinLock() {
        std::atomic_flag lock = ATOMIC_FLAG_INIT;
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &lock]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    while (lock.test_and_set(std::memory_order_acquire)) {
                        // Spin wait
                    }
                    results[i] = generateRandomChar();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    lock.clear(std::memory_order_release);
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "SpinLock Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Тест с SpinWait
    void testWithSpinWait() {
        std::atomic<bool> lock(false);
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &lock]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    bool expected = false;
                    while (!lock.compare_exchange_weak(expected, true, 
                            std::memory_order_acquire, std::memory_order_relaxed)) {
                        expected = false;
                        // Spin wait with potential yield
                        if (j % 100 == 0) {
                            std::this_thread::yield();
                        }
                    }
                    results[i] = generateRandomChar();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    lock.store(false, std::memory_order_release);
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "SpinWait Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Тест с монитором (условная переменная + мьютекс)
    void testWithMonitor() {
        std::mutex mtx;
        std::condition_variable cv;
        bool ready = true;
        results.assign(numThreads, ' ');
        threadTimes.assign(numThreads, 0);
        threads.clear();
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this, i, &mtx, &cv, &ready]() {
                auto threadStart = std::chrono::high_resolution_clock::now();
                
                for (int j = 0; j < raceLength; ++j) {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait(lock, [&ready]() { return ready; });
                    ready = false;
                    
                    results[i] = generateRandomChar();
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    
                    ready = true;
                    lock.unlock();
                    cv.notify_one();
                }
                
                auto threadEnd = std::chrono::high_resolution_clock::now();
                threadTimes[i] = std::chrono::duration_cast<std::chrono::microseconds>
                                (threadEnd - threadStart).count();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        std::cout << "Monitor Test - Total time: " << totalTime << " microseconds\n";
    }
    
    // Запуск всех тестов
    void runAllTests() {
        std::cout << "=== Running Thread Race Tests ===\n";
        std::cout << "Threads: " << numThreads << ", Race length: " << raceLength << "\n\n";
        
        testWithMutex();
        testWithSemaphore();
        testWithBarrier();
        testWithSpinLock();
        testWithSpinWait();
        testWithMonitor();
    }
};
