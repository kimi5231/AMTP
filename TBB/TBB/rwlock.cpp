#include <thread>
#include <iostream>
#include <vector>
#include <chrono>
#include <mutex>
#include <array>
#include <set>
#include <tbb/rw_mutex.h>

const int MAX_THREADS = 16;

class NODE {
public:
	int value;
	NODE* next;
	std::mutex node_lock;
	bool removed;
	NODE(int v) : value(v), next(nullptr), removed(false) {}
	void lock() { node_lock.lock(); }
	void unlock() { node_lock.unlock(); }
};

class DUMMY_MUTEX {
public:
	void lock() {}
	void unlock() {}
};

class C_SET {
	NODE* head, * tail;
	tbb::rw_mutex set_lock;
public:
	C_SET() {
		/* 값을 0부터 1000까지로 제한하겠음 */
		head = new NODE(-1);
		tail = new NODE(1000000001);
		head->next = tail;
	}

	~C_SET() {
		clear();
		delete head;
		delete tail;
	}

	void clear() {
		NODE* curr = head->next;
		while (curr != tail) {
			NODE* next = curr->next;
			delete curr;
			curr = next;
		}
		head->next = tail;
	}

	bool add(int x)
	{
		NODE* pred = head;
		set_lock.lock();
		NODE* curr = head->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->value == x) {
			set_lock.unlock();
			return false;
		}
		else
		{
			auto n = new NODE(x);
			n->next = curr;
			pred->next = n;
			set_lock.unlock();
			return true;
		}
	}

	bool remove(int x)
	{
		NODE* pred = head;
		set_lock.lock();
		NODE* curr = head->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->value == x) {
			pred->next = curr->next;
			set_lock.unlock();
			delete curr;
			return true;
		}
		else
		{
			set_lock.unlock();
			return false;
		}
	}
	bool contains(int x)
	{
		NODE* pred = head;
		set_lock.lock_shared();
		NODE* curr = head->next;
		while (curr->value < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->value == x) {
			set_lock.unlock_shared();
			return true;
		}
		else
		{
			set_lock.unlock_shared();
			return false;
		}
	}

	void print20()
	{
		NODE* curr = head->next;
		for (int i = 0; i < 20 && curr != tail; i++, curr = curr->next)
			std::cout << curr->value << ", ";
		std::cout << "\n";
	}
};

C_SET clist;

thread_local int thread_id;
thread_local int num_threads;

const int NUM_TEST = 4000000;
const int KEY_RANGE = 1000;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

std::array<std::vector<HISTORY>, MAX_THREADS> history;

void check_history(int num_threads)
{
	std::array <int, KEY_RANGE> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < KEY_RANGE; ++i) {
		int val = survive[i];
		if (val < 0) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			std::cout << "ERROR. The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (clist.contains(i)) {
				std::cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == clist.contains(i)) {
				std::cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	std::cout << " OK\n";
}

void benchmark_check(int num_threads, int th_id)
{
	thread_id = th_id;
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(0, v, clist.add(v));
			break;
		}
		case 1: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(1, v, clist.remove(v));
			break;
		}
		case 2: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(2, v, clist.contains(v));
			break;
		}
		}
	}
}


void benchmark(const int num_thread, int th_id)
{
	thread_id = th_id;
	const int loop_count = NUM_TEST / num_thread;

	for (int i = 0; i < loop_count; i++) {
		switch (rand() % 3) {
		case 0: {
			int key = rand() % KEY_RANGE;
			clist.add(key);
		}
			  break;
		case 1: {
			int key = rand() % KEY_RANGE;
			clist.remove(key);
		}
			  break;
		  default: {
			  int key = rand() % KEY_RANGE;
			  clist.contains(key);
			  }
		}
	}
}

int main()
{
	using namespace std::chrono;

	// 알고리즘 정확성 검사
	//std::cout << "Checking for consistency.\n\n";
	//{
	//	for (int i = 1; i <= MAX_THREADS; i = i * 2) {
	//		num_threads = i;
	//		std::vector <std::thread> threads;
	//		clist.clear();
	//		for (auto& h : history) h.clear();
	//		auto start_t = system_clock::now();
	//		for (int j = 0; j < i; ++j)
	//			threads.emplace_back(benchmark_check, i, j);
	//		for (auto& th : threads)
	//			th.join();
	//		auto end_t = system_clock::now();
	//		auto exec_t = end_t - start_t;
	//		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
	//		recycle_nodes();
	//		std::cout << i << " Threads : SET = ";
	//		clist.print20();
	//		std::cout << "Exec time = " << exec_ms << "ms.  ";
	//		check_history(i);
	//	}
	//}

	std::cout << "\nBenchmarking for speed.\n\n";
	for (num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		clist.clear();
		auto st = high_resolution_clock::now();
		std::vector<std::thread> threads;
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(benchmark, num_threads, i);
		for (int i = 0; i < num_threads; ++i)
			threads[i].join();
		auto ed = high_resolution_clock::now();
		auto time_span = duration_cast<milliseconds>(ed - st).count();
		//recycle_nodes();
		std::cout << "Thread Count = " << num_threads << ", Exec Time = " << time_span << "ms.\n";
		std::cout << "Result : ";  clist.print20();
	}
}
