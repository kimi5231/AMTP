#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>

class Bakery 
{
public:
	Bakery(int threadCount)
	{
		_flag.resize(threadCount);
		std::fill(_flag.begin(), _flag.end(), false);
		_label.resize(threadCount);
		std::fill(_label.begin(), _label.end(), 0);
	}

	void lock(int threadID)
	{
		// lock을 얻으면 flag를 활성화하고, 가장 큰 번호표를 부여받음
		_flag[threadID] = true;
		_label[threadID] = *(std::max_element(_label.begin(), _label.end())) + 1;

		// 자신의 차례가 올 때까지 기다리기
		for (int k = 0; k < _flag.size(); ++k)
		{
			while (_flag[k] && _label[k] < _label[threadID] && k < threadID) {}
		}
	}

	void unlock(int threadID)
	{
		_flag[threadID] = false;
	}

private:
	std::vector<bool> _flag;
	std::vector<int> _label;
};

volatile int sum;
std::atomic<int> atomicSum;

std::mutex m;

void Add(int threadCount)
{
	auto loop = 5000'0000 / threadCount;
	for (auto i = 0; i < loop; ++i)
		sum += 2;
}

void MutexAdd(int threadCount)
{
	auto loop = 5000'0000 / threadCount;
	for (auto i = 0; i < loop; ++i)
	{
		m.lock();
		sum += 2;
		m.unlock();
	}
}

void BakeryAdd(Bakery* bakery, int threadCount, int threadID)
{
	auto loop = 5000'0000 / threadCount;
	for (auto i = 0; i < loop; ++i)
	{
		bakery->lock(threadID);
		sum += 2;
		bakery->unlock(threadID);
	}
}

void BakeryAtomicAdd(Bakery* bakery, int threadCount, int threadID)
{
	auto loop = 5000'0000 / threadCount;
	for (auto i = 0; i < loop; ++i)
	{
		bakery->lock(threadID);
		atomicSum += 2;
		bakery->unlock(threadID);
	}
}

int main()
{
	// 아무것도 사용하지 않을 때
	{
		for (auto threadCount  : { 1, 2, 4, 8 })
		{
			sum = 0;
			std::vector<std::thread> threads;

			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < threadCount; ++i)
				threads.emplace_back(Add, threadCount);
			for (auto& thread : threads)
				thread.join();
			auto end = std::chrono::high_resolution_clock::now();

			std::cout << threadCount << " Threads Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}
	std::cout << std::endl;

	// mutex를 사용할 때
	{
		for (auto threadCount : { 1, 2, 4, 8 })
		{
			sum = 0;
			std::vector<std::thread> threads;

			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < threadCount; ++i)
				threads.emplace_back(MutexAdd, threadCount);
			for (auto& thread : threads)
				thread.join();
			auto end = std::chrono::high_resolution_clock::now();

			std::cout << threadCount << " Mutex Threads Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}
	std::cout << std::endl;

	// 빵집 알고리즘 <volatile>
	{
		for (auto threadCount : { 1, 2, 4, 8 })
		{
			sum = 0;
			Bakery* bakery = new Bakery(threadCount);
			std::vector<std::thread> threads;

			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < threadCount; ++i)
				threads.emplace_back(BakeryAdd, bakery, threadCount, i);
			for (auto& thread : threads)
				thread.join();
			auto end = std::chrono::high_resolution_clock::now();

			std::cout << threadCount << " Bakery Threads Sum = " << sum << std::endl;
			std::cout << "Time Taken = " << duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}
	std::cout << std::endl;

	// 빵집 알고리즘 <volatile을 atomic<T>로 대체>
	{
		for (auto threadCount : { 1, 2, 4, 8 })
		{
			atomicSum = 0;
			Bakery* bakery = new Bakery(threadCount);
			std::vector<std::thread> threads;

			auto start = std::chrono::high_resolution_clock::now();
			for (int i = 0; i < threadCount; ++i)
				threads.emplace_back(BakeryAtomicAdd, bakery, threadCount, i);
			for (auto& thread : threads)
				thread.join();
			auto end = std::chrono::high_resolution_clock::now();

			std::cout << threadCount << " Bakery Atomic Threads Sum = " << atomicSum << std::endl;
			std::cout << "Time Taken = " << duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;
		}
	}
}