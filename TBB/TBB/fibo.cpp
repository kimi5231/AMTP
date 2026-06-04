#include <tbb/task_group.h>
#include <iostream>
#include <chrono>

long SerialFib(volatile long n) {
    if (n < 2)
        return n;
    else
        return SerialFib(n - 1) + SerialFib(n - 2);
}

int parallel_fib(int n)
{
    if (n < 30)
        return SerialFib(n);
    else {
        int x, y;
        tbb::task_group g;
        g.run([&] {x = parallel_fib(n - 1); }); // spawn a task
        g.run([&] {y = parallel_fib(n - 2); }); // spawn another task
        g.wait();                // wait for both tasks to complete
        return x + y;
    }
}

int parallel_fib2(int n)
{
    if (n < 5)
        return SerialFib(n);
    else {
        int x, y;
        tbb::task_group g;
        g.run([&] {x = parallel_fib2(n - 1); }); // spawn a task
        g.run([&] {y = parallel_fib2(n - 2); }); // spawn another task
        g.wait();                // wait for both tasks to complete
        return x + y;
    }
}

int main()
{
    const int N = 40;
    auto start = std::chrono::high_resolution_clock::now();
    long serial_result = SerialFib(N);
    auto serial_duration = std::chrono::high_resolution_clock::now() - start;
    start = std::chrono::high_resolution_clock::now();
    std::cout << "Serial Fibonacci(" << N << ") = " << serial_result
        << " computed in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(serial_duration).count()
        << " ms\n"; 
    
    long parallel_result = parallel_fib(N);
    auto parallel_duration = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Parallel Fibonacci(" << N << ") = " << parallel_result
              << " computed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(parallel_duration).count()
              << " ms\n";

    long parallel_result2 = parallel_fib2(N);
    auto parallel_duration2 = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Parallel Fibonacci(" << N << ") = " << parallel_result2
        << " computed in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(parallel_duration2).count()
        << " ms\n";
    return 0;
}
