 // Работает по принципу семафора. Когда есть сигнал (то есть когда счётчик семафора равен единице), поток имеет доступ к операции. Когда сигнала нет, соответственно, доступа нет.

#include <iostream>
#include <thread>
#include <chrono>
#include <semaphore>
 
 
// глобальные экземпляры двоичных семафоров
// счетчик объектов установлен в ноль
// объекты в несигнальном состоянии
std::binary_semaphore
    smphSignalMainToThread(0),
    smphSignalThreadToMain(0);
 
void ThreadProc()
{    
    // ждем сигнала от main,
    // пытаясь уменьшить значение семафора
    smphSignalMainToThread.acquire();
 
    // этот вызов блокируется до тех пор, 
    // пока счетчик семафора не увеличится в main
 
    std::cout << "[thread] Got the signal\n"; // ответное сообщение

    // ждем 3 секунды для имитации какой-то работы,
    // выполняемой потоком
    using namespace std::literals;
    std::this_thread::sleep_for(3s);
 
    std::cout << "[thread] Send the signal\n"; // сообщение
 
    // сигнализируем обратно в main
    smphSignalThreadToMain.release();
}
 
int main()
{
    // создаем какой-то обрабатывающий поток
    std::thread thrWorker(ThreadProc);
 
    std::cout << "[main] Send the signal\n"; // сообщение
 
    // сигнализируем рабочему потоку о начале работы,
    // увеличивая значение счетчика семафора
    smphSignalMainToThread.release();
 
    // ждем, пока рабочий поток не выполнит свою работу,
    // пытаясь уменьшить значение счетчика семафора
    smphSignalThreadToMain.acquire();
 
    std::cout << "[main] Got the signal\n"; // ответное сообщение
    thrWorker.join();
}
/*
Вывод:
[main] Send the signal
[thread] Got the signal
[thread] Send the signal
[main] Got the signal
*/
