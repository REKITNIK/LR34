#pragma once

#include <benchmark/benchmark.h> 
#include "RaceTest.h"

class SynchronizationBenchmark {
public:
    static void BM_Mutex(benchmark::State& state) {
        ThreadRaceTest test(state.range(0), state.range(1));
        for (auto _ : state) {
            test.testWithMutex();
        }
    }
    
    static void BM_Semaphore(benchmark::State& state) {
        ThreadRaceTest test(state.range(0), state.range(1));
        for (auto _ : state) {
            test.testWithSemaphore();
        }
    }
    
    static void BM_SpinLock(benchmark::State& state) {
        ThreadRaceTest test(state.range(0), state.range(1));
        for (auto _ : state) {
            test.testWithSpinLock();
        }
    }
};

// Регистрация бенчмарков
BENCHMARK(SynchronizationBenchmark::BM_Mutex)
    ->Args({4, 100})
    ->Args({8, 100})
    ->Args({16, 100});

BENCHMARK(SynchronizationBenchmark::BM_Semaphore)
    ->Args({4, 100})
    ->Args({8, 100})
    ->Args({16, 100});

BENCHMARK(SynchronizationBenchmark::BM_SpinLock)
    ->Args({4, 100})
    ->Args({8, 100})
    ->Args({16, 100});
