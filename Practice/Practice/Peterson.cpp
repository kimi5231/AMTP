#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>

volatile int x, y;
constexpr int LOOP = 5000'0000;
int trace_x[LOOP], trace_y[LOOP];
std::mutex m;

void thread_x()
{
	for (auto i = 0; i < LOOP; ++i)
	{
		m.lock();
		sum = sum + 2;
		m.unlock();
	}
}

void thread_y()
{
	for (auto i = 0; i < LOOP; ++i)
	{
		y = i;
		trace_y[i] = y;
		sum = sum + 2;
		m.unlock();
	}
}

int main()
{
	std::thread t1(thread_x);
	std::thread t2(thread_y);

	t1.join();
	t2.join();

	int count = 0;

	for (auto i = 0; i < LOOP - 1; ++i)
	{
		if(trace_x[i] == trace_x[i + 1])
		{

	}
}