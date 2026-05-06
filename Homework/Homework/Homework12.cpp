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

class MEMORY_POOL {
private:
	std::queue<NODE*> get_pool;
	std::queue<NODE*> free_pool;
public:
	MEMORY_POOL()
	{
		//for (int i = 0; i < NUM_TEST / 3; ++i) {
		//	get_pool.push(new NODE(0));
		//}
	}
	~MEMORY_POOL() {
		while (!get_pool.empty()) {
			delete get_pool.front();
			get_pool.pop();
		}
		while (!free_pool.empty()) {
			delete free_pool.front();
			free_pool.pop();
		}
	}

	NODE* get_node(int value) {
		if (get_pool.empty()) {
			return new NODE(value);
		}
		else {
			NODE* node = get_pool.front();
			get_pool.pop();
			node->data = value;
			node->next = nullptr;
			return node;
		}
	}
	void free_node(NODE* node) {
		free_pool.push(node);
	}
	void recycle_nodes() {
		get_pool = std::move(free_pool);
	}
};

MEMORY_POOL memory_pool[MAX_THREADS];
thread_local int thread_id = 0;

class DUMMY_MUTEX {
public:
	void lock() {}
	void unlock() {}
};

class CQUEUE {
private:
	NODE *head, *tail;
	std::mutex enq_mtx, deq_mtx; // Mutex for thread safety
public:
	CQUEUE()
	{
		std::cout << "Testing Course Grain Synchronization Queue\n";
		tail = head = new NODE{ 0 };
	}

	void clear()
	{
		NODE* current = head->next;
		while (current != nullptr) {
			NODE* temp = current;
			current = current->next;
			delete temp;
		}
		head->next = nullptr;
		tail = head;
	}

	void Enqueue(int x)
	{
		NODE* new_node = new NODE(x);
		enq_mtx.lock(); // Lock the mutex to ensure thread safety
		tail->next = new_node;
		tail = new_node;
		enq_mtx.unlock(); // Unlock the mutex before returning
		return;
	}

	int Dequeue()
	{
		deq_mtx.lock(); // Lock the mutex to ensure thread safety
		if (head->next == nullptr) {
			deq_mtx.unlock(); // Unlock the mutex before returning
			return -1; // Queue is empty
		}
		int value = head->next->data;
		NODE* temp = head;
		head = head->next; // Move the head pointer to the next node
		deq_mtx.unlock(); // Unlock the mutex before returning
		delete temp; // Free the old head node
		return value;
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != nullptr && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class LFQUEUE {
private:
	NODE* volatile head, * volatile tail;
public:
	LFQUEUE()
	{
		std::cout << "Testing Lock Free Synchronization Queue\n";
		tail = head = new NODE{ 0 };
	}

	void clear()
	{
		NODE* current = head->next;
		while (current != nullptr) {
			NODE* temp = current;
			current = current->next;
			delete temp;
		}
		head->next = nullptr;
		tail = head;
	}

	bool CAS(NODE* volatile* addr, NODE* expected, NODE* new_node) {
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<NODE *> volatile *>(addr),
			&expected,
			new_node
		);
	}

	void Enqueue(int x)
	{
		NODE* new_node = new NODE(x);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;
			if (last == tail) {
				if (next == nullptr) {
					if (false == CAS(&last->next, nullptr, new_node))
						continue; // Failed to link the new node, retry
					CAS(&tail, last, new_node); // Try to swing the tail to the new node
					return; // Enqueue successful
				}
				else CAS(&tail, last, next);
			}
		}
	}

	int Dequeue()
	{
		while(true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first == head) {
				if (next == nullptr) return -1; // Queue is empty
				if (first == last) {
					CAS(&tail, last, next); // Try to swing the tail to the next node
				}
				else {
					int value = next->data;
					if (false == CAS(&head, first, next))
						continue; // Failed to swing the head to the next node, retry
					delete first; // Free the old head node
					return value; // Dequeue successful
				}
			}
		}
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != nullptr && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class LFNODE {
	std::atomic_llong next;
public:
	int data;
	long long epoch;
	LFNODE(int value) : data(value), next(0) {}
	void set_next(LFNODE* next_node) {
		next = reinterpret_cast<long long>(next_node);
	}
	LFNODE* get_next() {
		return reinterpret_cast<LFNODE*>(next.load() & 0xFFFFFFFFFFFFFFFC);
	}
	LFNODE* get_next(bool* removed) {
		long long temp = next.load();
		*removed = (temp & 1) == 1; // Check if the least significant bit is set (marked as removed)
		return reinterpret_cast<LFNODE*>(temp & 0xFFFFFFFFFFFFFFFC);
	}
	bool get_mark() {
		return (next.load() & 1) == 1; // Check if the least significant bit is set (marked as removed)
	}
	bool CAS(LFNODE* expected_node, LFNODE* new_node,
		bool expected_removed, bool new_removed)
	{
		long long expected_value = reinterpret_cast<long long>(expected_node) | (expected_removed ? 1 : 0);
		long long new_value = reinterpret_cast<long long>(new_node) | (new_removed ? 1 : 0);
		return next.compare_exchange_strong(expected_value, new_value);
	}
};

class LF_MEMORY_POOL {
private:
	std::queue<LFNODE*> get_pool;
	std::queue<LFNODE*> free_pool;
public:
	LF_MEMORY_POOL()
	{
		//for (int i = 0; i < NUM_TEST / 3; ++i) {
		//	get_pool.push(new NODE(0));
		//}
	}
	~LF_MEMORY_POOL() {
		while (!get_pool.empty()) {
			delete get_pool.front();
			get_pool.pop();
		}
		while (!free_pool.empty()) {
			delete free_pool.front();
			free_pool.pop();
		}
	}

	LFNODE* get_node(int value) {
		if (get_pool.empty()) {
			return new LFNODE(value);
		}
		else {
			LFNODE* node = get_pool.front();
			get_pool.pop();
			node->data = value;
			node->set_next(nullptr);
			return node;
		}
	}
	void free_node(LFNODE* node) {
		free_pool.push(node);
	}
	void recycle_nodes() {
		get_pool = std::move(free_pool);
	}
};

LF_MEMORY_POOL lf_memory_pool[MAX_THREADS];

class EBR {
	alignas(64) std::atomic_llong g_epoch = 0;
	class ThreadInfo {
	public:
		alignas(64) std::atomic_llong local_epoch;
		std::queue<LFNODE*> free_nodes;
		ThreadInfo() {
			local_epoch = std::numeric_limits<long long>::max(); // Initialize local_epoch to a value that indicates the thread is not active in any epoch
		}
		~ThreadInfo() {
			while (!free_nodes.empty()) {
				delete free_nodes.front();
				free_nodes.pop();
			}
		}
	};
	ThreadInfo thread_info[MAX_THREADS];
public:
	void enter() {
		thread_info[thread_id].local_epoch = ++g_epoch; // Mark the thread as active in the current global epoch
	}
	void leave() {
		thread_info[thread_id].local_epoch = std::numeric_limits<long long>::max(); // Mark the thread as inactive by setting local_epoch to a value that indicates it is not active in any epoch
	}
	void freenode(LFNODE* node) {
		node->epoch = g_epoch; // Set the node's epoch to the current global epoch
		thread_info[thread_id].free_nodes.push(node); // Add the node to the thread's free list
	}
	LFNODE* getnode(int x)
	{
		auto& free_queue = thread_info[thread_id].free_nodes;

		if (!free_queue.empty()) {
			LFNODE* node = free_queue.front();

			// 스레드별로 이전에 계산한 최솟값을 캐싱하여 스캔 비용 최소화
			static thread_local long long cached_min_epoch = 0;

			// 캐시된 최솟값으로 안전함을 보장할 수 없을 때만 전체 스레드 스캔
			if (node->epoch >= cached_min_epoch) {
				long long min_epoch = std::numeric_limits<long long>::max();
				for (int i = 0; i < MAX_THREADS; ++i) {
					long long t_epoch = thread_info[i].local_epoch.load();
					if (t_epoch < min_epoch) {
						min_epoch = t_epoch;
					}
				}
				cached_min_epoch = min_epoch;
			}

			// 갱신된 최솟값을 기준으로도 안전한 방출 선을 넘었다면 재사용
			if (node->epoch < cached_min_epoch) {
				free_queue.pop();
				node->data = x;
				node->set_next(nullptr);
				return node;
			}
		}

		// 재사용 가능한 노드가 없으면 새로 동적 할당
		return new LFNODE(x);
	}
};

EBR ebr;


LFQUEUE my_queue;

#include <array>
#include <unordered_set>

class HISTORY {
public:
	int op;
	int i_value;
	int o_value;
	HISTORY(int o, int i, int re) : op(o), i_value(i), o_value(re) {}
};

std::array<std::vector<HISTORY>, MAX_THREADS> history;

int num_threads = 0;

void check_history(int num_threads)
{
	std::unordered_multiset <int> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (op.op == 0) survive.insert(op.i_value);
		}
	}

	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (op.op == 1) {
				if (op.o_value == -1) continue;
				auto it = survive.find(op.o_value);
				if (it == survive.end()) {
					std::cout << "ERROR. The value " << op.o_value << " dequeued while it is not in the queue.\n";
					exit(-1);
				}
				survive.erase(it);
			}
		}
	}

	std::queue<int> copy_queues[MAX_THREADS];

	for (int i = 0; i < num_threads; ++i)
		for (auto& op : history[i])
			if (op.op == 1)
				if (op.o_value != -1)
					copy_queues[i].push(op.o_value);

	std::queue<int> backup_queue;
	for (;;) {
		auto x = my_queue.Dequeue();
		if (x == -1) break;
		backup_queue.push(x);
		auto it = survive.find(x);
		if (it == survive.end()) {
			std::cout << "ERROR. The value " << x << " dequeued while it is not in the queue.\n";
			exit(-1);
		}
		survive.erase(it);
	}

	if (survive.size() > 0) {
		std::cout << "ERROR. The value " << *survive.begin() << " should not be in the queue.\n";
		exit(-1);
	}

	//std::unordered_multiset<int> counter;
	//int index[MAX_THREADS] = { 0 };
	//for (int i = 0; i < num_threads; ++i)
	//	counter.insert(0);

	//while (true) {
	//	bool all_empty = true;
	//	bool exists = false;

	//	for (int i = 0; i < num_threads; ++i) {
	//		if (copy_queues[i].empty() == false) {
	//			all_empty = false;
	//			int x = copy_queues[i].front();
	//			auto it = counter.find(x);
	//			if (it != counter.end()) {
	//				copy_queues[i].pop();
	//				counter.erase(it);
	//				counter.insert(x + 1);
	//				exists = true;
	//			}
	//		}
	//	}
	//	if ((all_empty == true) && (backup_queue.empty() == true))
	//		break;

	//	if (false == exists) {
	//		if (backup_queue.empty() == true) {
	//			std::cout << "There is missing number in queue!\n";
	//			exit(-1);
	//		}
	//		int x = backup_queue.front();
	//		backup_queue.pop();
	//		auto it = counter.find(x);
	//		if (it != counter.end()) {
	//			counter.erase(it);
	//			counter.insert(x + 1);
	//			exists = true;
	//		}
	//		else {
	//			std::cout << "There is missing number in queue!\n";
	//			exit(-1);
	//		}
	//	}
	//}
	for (int i = 0; i < num_threads; ++i) history[i].clear();
	std::cout << " OK\n";
}

void benchmark_check(int num_threads, int th_id)
{
	thread_id = th_id;
	int key = 0;
	const int LOOP = NUM_TEST / num_threads;
	for (int i = 0; i < LOOP; i++) {
		if ((i < 32) || (rand() % 2 == 0)) {
			history[th_id].emplace_back(0, key, -2);
			my_queue.Enqueue(key++);
		}
		else {
			history[th_id].emplace_back(1, -1, my_queue.Dequeue());
		}
	}
}

void benchmark(int num_threads, int tid)
{
	thread_id = tid;
	int key = 0;
	const int LOOP = NUM_TEST / num_threads;
	for (int i = 0; i < LOOP; i++) {
		if ((i < 32) || (rand() % 2 == 0))
			my_queue.Enqueue(key++);
		else
			my_queue.Dequeue();
	}
}

int main()
{
	using namespace std::chrono;


	std::cout << "Strting Error Check.\n";
	for (num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
		std::vector<std::thread> threads;
		for (auto& h : history) h.clear();
		auto start_time = high_resolution_clock::now();
		for (int j = 0; j < num_threads; ++j) {
			threads.emplace_back(benchmark_check, num_threads, j);
		}
		for (auto& thread : threads) {
			thread.join();
		}
		auto end_time = high_resolution_clock::now();
		auto elapsed = end_time - start_time;
		auto exec_ms = duration_cast<milliseconds>(elapsed).count();
		my_queue.print20();
		std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";
		check_history(num_threads);
		my_queue.clear();
	}


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
		my_queue.print20();
		std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";
		my_queue.clear();
		//std::string temp;
		//std::getline(std::cin, temp);
	}
}