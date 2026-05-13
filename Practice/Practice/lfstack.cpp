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
		NODE* new_node = new NODE(x);
		mtx.lock(); // Lock the mutex to ensure thread safety	
		new_node->next = top; // Point the new node to the current top
		top = new_node; // Update the top pointer to the new node
		mtx.unlock(); // Unlock the mutex before returning
	}

	int Pop()
	{
		mtx.lock(); // Lock the mutex to ensure thread safety
		if (top == nullptr) {
			mtx.unlock(); // Unlock the mutex before returning
			return -2; // Stack is empty
		}
		int value = top->data;
		NODE* temp = top;
		top = top->next; // Move the top pointer to the next node
		mtx.unlock(); // Unlock the mutex before returning
		delete temp; // Free the old top node
		return value;
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

class LFSTACK {
private:
	NODE* volatile top;
public:
	bool CAS(NODE * volatile * addr, NODE* expected, NODE* new_node) {
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE *> volatile *>(addr),
			&expected,
			new_node
		);
	}
	LFSTACK()
	{
		std::cout << "Testing Lock Free Synchronization Stack\n";
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
		NODE* new_node = new NODE(x);
		if (top != nullptr)
			new_node->next = top;
		if (CAS(&new_node->next, top, top))
			top = new_node;
	}

	int Pop()
	{
		if (top == nullptr)
			return -2;

		NODE* node = top->next;
		if (node != nullptr)
		{
			if (CAS(&node, top->next, top->next))
				top->next = node->next;
			delete node;
		}
		
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

LFSTACK my_stack;

#include <array>
#include <unordered_set>
#include <set>

int num_threads = 0;

struct HISTORY {
	std::vector <int> push_values, pop_values;
};
std::atomic_int stack_size;

void benchmark(const int num_thread)
{
	int key = 0;
	const int loop_count = NUM_TEST / num_thread;
	for (auto i = 0; i < loop_count; ++i) {
		if ((rand() % 2 == 0) || (i < 1000))
			my_stack.Push(key++);
		else
			my_stack.Pop();
	}
}

void benchmark_test(const int th_id, const int num_threads, HISTORY& h)
{
	thread_id = th_id;
	int loop_count = NUM_TEST / num_threads;
	for (int i = 0; i < loop_count; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			h.push_values.push_back(i);
			stack_size++;
			my_stack.Push(i);
		}
		else {
			volatile int curr_size = stack_size--;
			int res = my_stack.Pop();
			if (res == -2) {
				stack_size++;
				if ((curr_size > num_threads * 2) && (stack_size > num_threads)) {
					std::cout << "ERROR Non_Empty Stack Returned NULL\n";
					exit(-1);
				}
			}
			else h.pop_values.push_back(res);
		}
	}
}

void check_history(std::vector <HISTORY>& h)
{
	std::unordered_multiset <int> pushed, poped, in_stack;

	for (auto& v : h)
	{
		for (auto num : v.push_values) pushed.insert(num);
		for (auto num : v.pop_values) poped.insert(num);
		while (true) {
			int num = my_stack.Pop();
			if (num == -2) break;
			poped.insert(num);
		}
	}
	for (auto num : pushed) {
		if (poped.count(num) < pushed.count(num)) {
			std::cout << "Pushed Number " << num << " does not exists in the STACK.\n";
			exit(-1);
		}
		if (poped.count(num) > pushed.count(num)) {
			std::cout << "Pushed Number " << num << " is poped more than " << poped.count(num) - pushed.count(num) << " times.\n";
			exit(-1);
		}
	}
	for (auto num : poped)
		if (pushed.count(num) == 0) {
			std::multiset <int> sorted;
			for (auto num : poped)
				sorted.insert(num);
			std::cout << "There were elements in the STACK no one pushed : ";
			int count = 20;
			for (auto num : sorted)
				std::cout << num << ", ";
			std::cout << std::endl;
			exit(-1);

		}
	std::cout << "NO ERROR detectd.\n";
}

int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		num_threads = n;
		my_stack.clear();
		std::vector<std::thread> tv;
		std::vector<HISTORY> history;
		history.resize(n);
		stack_size = 0;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			tv.emplace_back(benchmark_test, i, n, std::ref(history[i]));
		}
		for (auto& th : tv)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();
		std::cout << n << " Threads,  " << ms << "ms. ----";
		my_stack.print20();
		check_history(history);
	}

	for (int n = 1; n <= MAX_THREADS; n *= 2) {
		num_threads = n;
		my_stack.clear();
		std::vector<std::thread> tv;
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < n; ++i) {
			tv.emplace_back(benchmark, n);
		}
		for (auto& th : tv)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();
		std::cout << n << " Threads,  " << ms << "ms. ----";
		my_stack.print20();
	}
}