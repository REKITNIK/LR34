#include "benchmark.h"
#include "RaceTest.h"

int main() {
    // Простое тестирование
    ThreadRaceTest simpleTest(8, 500);
    simpleTest.runAllTests();
    
    // Тестирование с разным количеством потоков
    std::cout << "\n\n=== Testing with different thread counts ===\n";
    
    for (int threads : {2, 4, 8, 16, 32}) {
        std::cout << "\n--- " << threads << " threads ---\n";
        ThreadRaceTest test(threads, 200);
        test.testWithMutex();
        test.testWithSpinLock();
        test.testWithSpinWait();
    }
    
    return 0;
}
