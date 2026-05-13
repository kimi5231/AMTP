#include <iostream>

#include <thread>

#include <vector>

#include <chrono>

#include <mutex>

#include <queue>



constexpr int MAX_THREADS = 32;

constexpr int NUM_TEST = 1000'0000;



class NODE {

public:

	int data;

	NODE* next;

	NODE(int value) : data(value), next(nullptr) {}

};



thread_local int thread_id = 0;



class DUMMY_MUTEX {

public:

	void lock() {}

	void unlock() {}

};



class CSTACK {

private:

	NODE* top;

	std::mutex mtx; // Mutex for thread safety

public:
	CSTACK()
	{
		std::cout << "Testing Course Grain Synchronization Stack\n";
		top = nullptr;
	}

	void clear()
	{
		while (top != nullptr) {
			NODE* temp = top;
			top = top->next;
			delete temp;
		}
	}

	void Push(int x)
	{
		mtx.lock();
		NODE* newNode = new NODE(x);
		if (top != nullptr)
			newNode->next = top;
		top = newNode;
		mtx.unlock();
	}

	int Pop()
	{
		if (top == nullptr)
			return -2;

		mtx.lock();
		NODE* node = top->next;
		top->next = node->next;
		delete node;
		mtx.unlock();

		return 1;
	}

	void print20()
	{
		NODE* curr = top;
		int count = 0;

		while (curr != nullptr && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}

		std::cout << "\n";

	}

};

CSTACK my_stack;

#include <array>
#include <unordered_set>

int num_threads = 0;

void benchmark(int num_threads, int tid)

{

	thread_id = tid;

	int key = 0;

	const int LOOP = NUM_TEST / num_threads;

	for (int i = 0; i < LOOP; i++) {

		if ((i < 1000) || (rand() % 2 == 0))

			my_stack.Push(key++);

		else

			my_stack.Pop();

	}

}



int main()

{

	using namespace std::chrono;



	std::cout << "Strting Performance Check.\n";

	for (num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {

		std::vector<std::thread> threads;

		auto start_time = high_resolution_clock::now();

		for (int j = 0; j < num_threads; ++j) {

			threads.emplace_back(benchmark, num_threads, j);

		}

		for (auto& thread : threads) {

			thread.join();

		}

		auto end_time = high_resolution_clock::now();

		auto elapsed = end_time - start_time;

		auto exec_ms = duration_cast<milliseconds>(elapsed).count();

		my_stack.print20();

		std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";

		my_stack.clear();

		//std::string temp;

		//std::getline(std::cin, temp);

	}

}