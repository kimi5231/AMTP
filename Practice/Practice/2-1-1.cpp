#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>

volatile int sum;
std::atomic<int> atomic_sum;
std::mutex m;

constexpr int MAX_THREADS = 8;
volatile int array_sum[MAX_THREADS];

void array_add(int num_thread, int thread_id)
{
	auto loop = 5000'0000 / num_thread;
	for (auto i = 0; i < loop; ++i)
	{
		array_sum[thread_id] = array_sum[thread_id] + 2;
	}
}

void add(int num_thread)
{
	auto loop = 5000'0000 / num_thread;
	for (auto i = 0; i < loop; ++i)
	{
		m.lock();
		sum = sum + 2;
		m.unlock();
	}
}

void atomic_add(int num_thread)
{
	auto loop = 5000'0000 / num_thread;
	for (auto i = 0; i < loop; ++i)
	{
		//m.lock();
		// 의미X
		//atomic_sum = atomic_sum + 2;
		atomic_sum += 2;
		//m.unlock();
	}
}

void optimal_add(int num_thread)
{
	auto loop = 5000'0000 / num_thread;
	volatile int local_sum = 0;
	for (auto i = 0; i < loop; ++i)
		local_sum = local_sum + 2;
	m.lock();
	sum += local_sum;
	m.unlock();
}

int main()
{
	using namespace std::chrono;

	{
		auto start = high_resolution_clock::now();
		for (auto i = 0; i < 5000'0000; ++i)
			sum = sum + 2;
		auto end = high_resolution_clock::now();
		std::cout << "Single Thread Sum = " << sum << std::endl;
		std::cout << "Time Taken = " << duration_cast<milliseconds>(end - start).count() << "ms" << std::endl;
	}
	
	{
		for (auto num_threads : { 1, 2, 4, 6, 8 })
		{
			sum = 0;
			std::vector<std::thread> threads;
			auto start = high_resolution_clock::now();
			for (int i = 0; i < num_threads; ++i)
				threads.emplace_back(add, num_threads);
			for (auto& thread : threads)
				thread.join();
			auto end = high_resolution_clock::now();
			std::cout << num_threads << " Threads Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}

	{
		for (auto num_threads : { 1, 2, 4, 6, 8 })
		{
			atomic_sum = 0;
			std::vector<std::thread> threads;
			auto start = high_resolution_clock::now();
			for (int i = 0; i < num_threads; ++i)
				threads.emplace_back(atomic_add, num_threads);
			for (auto& thread : threads)
				thread.join();
			auto end = high_resolution_clock::now();
			std::cout << num_threads << " Atomic Threads Sum = " << atomic_sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}

	{
		for (auto num_threads : { 1, 2, 4, 6, 8 })
		{
			sum = 0;
			std::vector<std::thread> threads;
			auto start = high_resolution_clock::now();
			for (int i = 0; i < num_threads; ++i)
				threads.emplace_back(optimal_add, num_threads);
			for (auto& thread : threads)
				thread.join();
			auto end = high_resolution_clock::now();
			std::cout << num_threads << " Optimal Threads Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}

	{
		for (auto num_threads : { 1, 2, 4, 6, 8 })
		{
			for (auto& s : array_sum)
				s = 0;
			std::vector<std::thread> threads;
			auto start = high_resolution_clock::now();
			for (int i = 0; i < num_threads; ++i)
				threads.emplace_back(array_add, num_threads, i);
			for (auto& thread : threads)
				thread.join();
			auto end = high_resolution_clock::now();
			sum = 0;
			for (auto& s : array_sum)
				sum += s;
			std::cout << num_threads << " Threads Array Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}
}