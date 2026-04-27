#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <atomic>
#include <limits>
#include <array>
#include <algorithm>

constexpr int MAX_THREADS = 16;
constexpr int NUM_TEST = 400'0000;
constexpr int RANGE = 1000;

// --- 공통 노드 및 메모리 풀 구조 ---

class NODE {
public:
	int data;
	NODE* next;
	bool removed = false;
	std::mutex mtx;
	NODE(int value) : data(value), next(nullptr) {}
	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class MEMORY_POOL {
private:
	std::queue<NODE*> get_pool;
	std::queue<NODE*> free_pool;
public:
	NODE* get_node(int value) {
		if (get_pool.empty()) return new NODE(value);
		NODE* node = get_pool.front(); get_pool.pop();
		node->data = value; node->next = nullptr; node->removed = false;
		return node;
	}
	void free_node(NODE* node) { free_pool.push(node); }
	void recycle_nodes() { while (!free_pool.empty()) { get_pool.push(free_pool.front()); free_pool.pop(); } }
};

MEMORY_POOL memory_pool[MAX_THREADS + 1];
thread_local int thread_id = MAX_THREADS; // 기본값은 메인 스레드용

// --- Lock-Free를 위한 노드 (LFNODE) ---

class LFNODE {
	std::atomic_llong next;
public:
	int data;
	long long epoch;
	LFNODE(int value) : data(value), next(0), epoch(0) {}
	void set_next(LFNODE* next_node) { next = reinterpret_cast<long long>(next_node); }
	LFNODE* get_next() { return reinterpret_cast<LFNODE*>(next.load() & ~3LL); }
	LFNODE* get_next(bool* removed) {
		long long temp = next.load();
		*removed = (temp & 1) == 1;
		return reinterpret_cast<LFNODE*>(temp & ~3LL);
	}
	bool get_mark() { return (next.load() & 1) == 1; }
	bool CAS(LFNODE* exp_node, LFNODE* new_node, bool exp_rem, bool new_rem) {
		long long exp_val = reinterpret_cast<long long>(exp_node) | (exp_rem ? 1 : 0);
		long long new_val = reinterpret_cast<long long>(new_node) | (new_rem ? 1 : 0);
		return next.compare_exchange_strong(exp_val, new_val);
	}
};

// --- EBR (Epoch Based Reclamation) 구현 ---

class EBR {
	alignas(64) std::atomic_llong g_epoch{ 0 };
	struct ThreadInfo {
		alignas(64) std::atomic_llong local_epoch{ std::numeric_limits<long long>::max() };
		std::queue<LFNODE*> free_nodes;
	};
	ThreadInfo thread_info[MAX_THREADS + 1];

public:
	void enter() {
		thread_info[thread_id].local_epoch.store(g_epoch.load());
	}
	void leave() {
		thread_info[thread_id].local_epoch.store(std::numeric_limits<long long>::max());
	}
	void freenode(LFNODE* node) {
		node->epoch = g_epoch.load();
		thread_info[thread_id].free_nodes.push(node);
		if (thread_info[thread_id].free_nodes.size() % 100 == 0) g_epoch.fetch_add(1);
	}
	LFNODE* getnode(int x) {
		if (thread_info[thread_id].free_nodes.empty()) return new LFNODE(x);
		LFNODE* node = thread_info[thread_id].free_nodes.front();
		for (int i = 0; i <= MAX_THREADS; ++i) {
			if (thread_info[i].local_epoch.load() <= node->epoch) return new LFNODE(x);
		}
		thread_info[thread_id].free_nodes.pop();
		node->data = x;
		node->set_next(nullptr);
		return node;
	}
};

EBR ebr;

// --- LFEBRLIST (EBR 기반 락프리 리스트) ---

class LFEBRLIST {
	LFNODE* head, * tail;
public:
	LFEBRLIST() {
		head = new LFNODE(std::numeric_limits<int>::min());
		tail = new LFNODE(std::numeric_limits<int>::max());
		head->set_next(tail);
	}
	void clear() {
		LFNODE* curr = head->get_next();
		while (curr != tail) {
			LFNODE* next = curr->get_next();
			delete curr;
			curr = next;
		}
		head->set_next(tail);
	}

	void find(int x, LFNODE*& pred, LFNODE*& curr) {
	retry:
		pred = head;
		curr = pred->get_next();
		while (curr != tail) {
			bool removed = false;
			LFNODE* succ = curr->get_next(&removed);
			if (removed) {
				if (!pred->CAS(curr, succ, false, false)) goto retry;
				ebr.freenode(curr);
				curr = succ;
				continue;
			}
			if (curr->data >= x) break;
			pred = curr;
			curr = succ;
		}
	}

	bool Add(int x) {
		ebr.enter();
		while (true) {
			LFNODE* pred, * curr;
			find(x, pred, curr);
			if (curr->data == x) { ebr.leave(); return false; }
			LFNODE* new_node = ebr.getnode(x);
			new_node->set_next(curr);
			if (pred->CAS(curr, new_node, false, false)) { ebr.leave(); return true; }
			ebr.freenode(new_node);
		}
	}

	bool Remove(int x) {
		ebr.enter();
		while (true) {
			LFNODE* pred, * curr;
			find(x, pred, curr);
			if (curr->data != x) { ebr.leave(); return false; }
			LFNODE* succ = curr->get_next();
			if (!curr->CAS(succ, succ, false, true)) continue;
			if (pred->CAS(curr, succ, false, false)) ebr.freenode(curr);
			ebr.leave(); return true;
		}
	}

	bool Contains(int x) {
		ebr.enter();
		LFNODE* curr = head->get_next();
		while (curr != tail && curr->data < x) curr = curr->get_next();
		bool res = (curr != tail && curr->data == x && !curr->get_mark());
		ebr.leave();
		return res;
	}

	void print20() {
		LFNODE* curr = head->get_next();
		for (int i = 0; i < 20 && curr != tail; ++i) {
			std::cout << curr->data << ", ";
			curr = curr->get_next();
		}
		std::cout << "\n";
	}
};

LFEBRLIST my_set;

// --- 벤치마크 및 테스트 로직 ---

struct HISTORY {
	int op; int i_value; bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};
std::array<std::vector<HISTORY>, MAX_THREADS> history;

void check_history(int num_threads) {
	std::array<int, RANGE> survive = {};
	std::cout << "Checking Consistency : ";
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (!op.o_value) continue;
			if (op.op == 0) survive[op.i_value]++;
			else if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < RANGE; ++i) {
		if (survive[i] < 0 || survive[i] > 1) { std::cout << "ERROR at " << i << "\n"; exit(-1); }
		if (survive[i] == 1 && !my_set.Contains(i)) { std::cout << "MISSING " << i << "\n"; exit(-1); }
		if (survive[i] == 0 && my_set.Contains(i)) { std::cout << "SURPLUS " << i << "\n"; exit(-1); }
	}
	std::cout << " OK\n";
}

void benchmark_check(int num_threads, int tid) {
	thread_id = tid;
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3; int v = rand() % RANGE;
		if (op == 0) history[tid].emplace_back(0, v, my_set.Add(v));
		else if (op == 1) history[tid].emplace_back(1, v, my_set.Remove(v));
		else history[tid].emplace_back(2, v, my_set.Contains(v));
	}
}

void benchmark(int num_threads, int tid) {
	thread_id = tid;
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3; int v = rand() % RANGE;
		if (op == 0) my_set.Add(v);
		else if (op == 1) my_set.Remove(v);
		else my_set.Contains(v);
	}
}

int main() {
	using namespace std::chrono;
	std::cout << "Starting Error Check.\n";
	for (int n = 1; n <= MAX_THREADS; n *= 2) {
		for (auto& h : history) h.clear();
		std::vector<std::thread> ths;
		auto start = high_resolution_clock::now();
		for (int j = 0; j < n; ++j) ths.emplace_back(benchmark_check, n, j);
		for (auto& t : ths) t.join();
		auto dur = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
		my_set.print20();
		std::cout << "Threads: " << n << ", Time: " << dur << "ms\n";
		check_history(n);
		my_set.clear();
	}

	std::cout << "Starting Performance Check.\n";
	for (int n = 1; n <= MAX_THREADS; n *= 2) {
		std::vector<std::thread> ths;
		auto start = high_resolution_clock::now();
		for (int j = 0; j < n; ++j) ths.emplace_back(benchmark, n, j);
		for (auto& t : ths) t.join();
		auto dur = duration_cast<milliseconds>(high_resolution_clock::now() - start).count();
		my_set.print20();
		std::cout << "Threads: " << n << ", Time: " << dur << "ms\n";
		my_set.clear();
	}
	return 0;
}