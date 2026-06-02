#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <queue>
#include <set>
#include <random>
#include <atomic>

constexpr int MAX_THREADS = 32;
constexpr int NUM_TEST = 400'0000;
constexpr int RANGE = 1000;



class NODE {
public:
	int data;
	NODE* next;
	bool removed = false; // Flag to indicate if the node is removed
	std::mutex mtx; // Mutex for thread safety
	NODE(int value) : data(value), next(nullptr) {}
	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
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
			node->removed = false;
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
thread_local std::mt19937 rng{ std::random_device{}() };

class DUMMY_MUTEX {
	public:
		void lock() {}
		void unlock() {}
};

class CLIST {
private:
	NODE* head, * tail;
	std::mutex mtx; // Mutex for thread safety
public:
	CLIST()
	{
		std::cout << "Testing Course Grain Synchronization List\n";
		head = new NODE{ std::numeric_limits<int>::min() };
		tail = new NODE{ std::numeric_limits<int>::max() };
		head->next = tail;
	}

	void clear()
	{
		NODE* current = head->next;
		while (head->next != tail) {
			NODE* temp = head->next;
			head->next = temp->next;
			delete temp;
		}
	}

	bool Add(int x)
	{
		mtx.lock(); // Lock the mutex to ensure thread safety
		NODE* pred = head;
		NODE* curr = pred->next;
		while (curr->data < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->data == x) {
			mtx.unlock(); // Unlock the mutex before returning
			return false; // Element already exists
		}
		else {
			NODE* new_node = memory_pool[thread_id].get_node(x);
			pred->next = new_node;
			new_node->next = curr;
			mtx.unlock(); // Unlock the mutex after modifying the list
			return true; // Element added successfully
		}
	}

	bool Remove(int x)
	{

		mtx.lock(); // Lock the mutex to ensure thread safety
		NODE* pred = head;
		NODE* curr = pred->next;
		while (curr->data < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->data != x) {
			mtx.unlock(); // Unlock the mutex before returning
			return false; // Element already exists
		}
		else {
			pred->next = curr->next;
			memory_pool[thread_id].free_node(curr); // Recycle the removed node back to the memory pool
			mtx.unlock(); // Unlock the mutex after modifying the list
			return true; // Element added successfully
		}
	}

	bool Contains(int x)
	{

		mtx.lock(); // Lock the mutex to ensure thread safety
		NODE* pred = head;
		NODE* curr = pred->next;
		while (curr->data < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->data == x) {
			mtx.unlock(); // Unlock the mutex before returning
			return true; // Element already exists
		}
		else {
			mtx.unlock(); // Unlock the mutex after modifying the list
			return false; // Element added successfully
		}
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr= curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class FLIST {
private:
	NODE* head, * tail;
public:
	FLIST()
	{
		std::cout << "Testing Fine Grain Synchronization List\n";
		head = new NODE{ std::numeric_limits<int>::min() };
		tail = new NODE{ std::numeric_limits<int>::max() };
		head->next = tail;
	}

	void clear()
	{
		NODE* current = head->next;
		while (head->next != tail) {
			NODE* temp = head->next;
			head->next = temp->next;
			delete temp;
		}
	}

	bool Add(int x)
	{
		head->lock();
		NODE* pred = head;
		NODE* curr = pred->next;
		curr->lock();
		while (curr->data < x) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->data == x) {
			pred->unlock(); curr->unlock(); // Unlock the mutex before returning
			return false; // Element already exists
		}
		else {
			NODE* new_node = new NODE{ x };
			new_node->next = curr;
			pred->next = new_node;
			pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
			return true; // Element added successfully
		}
	}

	bool Remove(int x)
	{
		head->lock();
		NODE* pred = head;
		NODE* curr = pred->next;
		curr->lock();
		while (curr->data < x) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->data != x) {
			pred->unlock(); curr->unlock(); // Unlock the mutex before returning
			return false; // Element already exists
		}
		else {
			pred->next = curr->next;
			pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
			delete curr;
			return true; // Element added successfully
		}
	}

	bool Contains(int x)
	{
		head->lock();
		NODE* pred = head;
		NODE* curr = pred->next;
		curr->lock();
		while (curr->data < x) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (curr->data == x) {
			pred->unlock(); curr->unlock(); // Unlock the mutex before returning
			return true; // Element already exists
		}
		else {
			pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
			return false; // Element added successfully
		}
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class OLIST {
private:
	NODE* head, * tail;
public:
	OLIST()
	{
		std::cout << "Testing Optimistic Synchronization List\n";
		head = new NODE{ std::numeric_limits<int>::min() };
		tail = new NODE{ std::numeric_limits<int>::max() };
		head->next = tail;
	}

	void clear()
	{
		NODE* current = head->next;
		while (head->next != tail) {
			NODE* temp = head->next;
			head->next = temp->next;
			delete temp;
		}
	}

	bool validate(NODE* pred, NODE* curr)
	{
		NODE* node = head;
		while (node->data <= pred->data) {
			if (node == pred) {
				return pred->next == curr; // Check if pred still points to curr
			}
			node = node->next;
		}
		return false; // Validation failed
	}

	bool Add(int x)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}

			if (curr->data == x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				NODE* new_node = memory_pool[thread_id].get_node(x);
				new_node->next = curr;
				pred->next = new_node;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Remove(int x)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}
			if (curr->data != x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				pred->next = curr->next;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				memory_pool[thread_id].free_node(curr); // Recycle the removed node back to the memory pool
				return true; // Element added successfully
			}
		}
	}

	bool Contains(int x)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}

			if (curr->data == x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return true; // Element already exists
			}
			else {
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return false; // Element added successfully
			}
		}
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class ZLIST {
private:
	NODE* head, * tail;
public:
	ZLIST()
	{
		std::cout << "Testing Optimistic Synchronization List\n";
		head = new NODE{ std::numeric_limits<int>::min() };
		tail = new NODE{ std::numeric_limits<int>::max() };
		head->next = tail;
	}

	void clear()
	{
		NODE* current = head->next;
		while (head->next != tail) {
			NODE* temp = head->next;
			head->next = temp->next;
			delete temp;
		}
	}

	bool validate(NODE* pred, NODE* curr)
	{
		return (!pred->removed) && (!curr->removed) 
			&& (pred->next == curr); // Check if pred still points to curr and both nodes are not removed
	}

	bool Add(int x)
	{
		NODE* new_node = memory_pool[thread_id].get_node(x);
		while (true) {
			NODE* pred = head;
			NODE* curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}

			if (curr->data == x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				memory_pool[thread_id].free_node(new_node); // Recycle the unused node back to the memory pool
				return false; // Element already exists
			}
			else {
				new_node->next = curr;
				pred->next = new_node;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Remove(int x)
	{
		while (true) {
			NODE* pred = head;
			NODE* curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}
			if (curr->data != x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				curr->removed = true; // Mark the node as removed
				pred->next = curr->next;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				memory_pool[thread_id].free_node(curr); // Recycle the removed node back to the memory pool
				return true; // Element added successfully
			}
		}
	}

	bool Contains(int x)
	{
		NODE* n = head;
		while (n->data < x) {
			n = n->next;
		}
		return (n->data == x) && (!n->removed); // Check if the node exists and is not removed
	}

	void print20()
	{
		NODE* curr = head->next;
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class NODE_SP {
public:
	int data;
	std::shared_ptr<NODE_SP> next;
	bool removed = false; // Flag to indicate if the node is removed
	std::mutex mtx; // Mutex for thread safety
	NODE_SP(int value) : data(value), next(nullptr) {}
	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class ZLIST_SP {
private:
	std::shared_ptr<NODE_SP> head, tail;
public:
	ZLIST_SP()
	{
		std::cout << "Testing Optimistic Synchronization List\n";
		head = std::make_shared<NODE_SP>( std::numeric_limits<int>::min() );
		tail = std::make_shared<NODE_SP>( std::numeric_limits<int>::max() );
		head->next = tail;
	}

	void clear()
	{
		head->next = tail; // Reset the list to its initial state
	}

	bool validate(const std::shared_ptr<NODE_SP> &pred, 
		const std::shared_ptr<NODE_SP> &curr)
	{
		return (!pred->removed) && (!curr->removed)
			&& (pred->next == curr); // Check if pred still points to curr and both nodes are not removed
	}

	bool Add(int x)
	{
		std::shared_ptr<NODE_SP> new_node = std::make_shared<NODE_SP>(x);
		while (true) {
			std::shared_ptr<NODE_SP> pred = head;
			std::shared_ptr<NODE_SP> curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}

			if (curr->data == x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				new_node->next = curr;
				pred->next = new_node;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Remove(int x)
	{
		while (true) {
			std::shared_ptr<NODE_SP> pred = head;
			std::shared_ptr<NODE_SP> curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}
			if (curr->data != x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				curr->removed = true; // Mark the node as removed
				pred->next = curr->next;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Contains(int x)
	{
		std::shared_ptr<NODE_SP> n = head;
		while (n->data < x) {
			n = n->next;
		}
		return (n->data == x) && (!n->removed); // Check if the node exists and is not removed
	}

	void print20()
	{
		std::shared_ptr<NODE_SP> curr = head->next;
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->next;
			count++;
		}
		std::cout << "\n";
	}
};

class NODE_ASP {
public:
	int data;
	std::atomic<std::shared_ptr<NODE_ASP>> next;
	bool removed = false; // Flag to indicate if the node is removed
	std::mutex mtx; // Mutex for thread safety
	NODE_ASP(int value) : data(value), next(nullptr) {}
	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class ZLIST_ASP {
private:
	std::atomic<std::shared_ptr<NODE_ASP>> head, tail;
public:
	ZLIST_ASP()
	{
		std::cout << "Testing Optimistic Synchronization List\n";
		head = std::make_shared<NODE_ASP>(std::numeric_limits<int>::min());
		tail = std::make_shared<NODE_ASP>(std::numeric_limits<int>::max());
		head.load()->next = tail.load();
	}

	void clear()
	{
		head.load()->next = tail.load(); // Reset the list to its initial state
	}

	bool validate(const std::shared_ptr<NODE_ASP>& pred,
		const std::shared_ptr<NODE_ASP>& curr)
	{
		return (!pred->removed) && (!curr->removed)
			&& (pred->next.load() == curr); // Check if pred still points to curr and both nodes are not removed
	}

	bool Add(int x)
	{
		std::shared_ptr<NODE_ASP> new_node = std::make_shared<NODE_ASP>(x);
		while (true) {
			std::shared_ptr<NODE_ASP> pred = head;
			std::shared_ptr<NODE_ASP> curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}

			if (curr->data == x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				new_node->next = curr;
				pred->next = new_node;
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Remove(int x)
	{
		while (true) {
			std::shared_ptr<NODE_ASP> pred = head;
			std::shared_ptr<NODE_ASP> curr = pred->next;
			while (curr->data < x) {
				pred = curr;
				curr = curr->next;
			}

			pred->lock(); 	curr->lock();
			if (false == validate(pred, curr)) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before retrying
				continue;
			}
			if (curr->data != x) {
				pred->unlock(); curr->unlock(); // Unlock the mutex before returning
				return false; // Element already exists
			}
			else {
				curr->removed = true; // Mark the node as removed
				pred->next = curr->next.load();
				pred->unlock(); curr->unlock(); // Unlock the mutex after modifying the list
				return true; // Element added successfully
			}
		}
	}

	bool Contains(int x)
	{
		std::shared_ptr<NODE_ASP> n = head;
		while (n->data < x) {
			n = n->next;
		}
		return (n->data == x) && (!n->removed); // Check if the node exists and is not removed
	}

	void print20()
	{
		std::shared_ptr<NODE_ASP> curr = head.load()->next;
		int count = 0;
		while (curr != tail.load() && count < 20) {
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

class LFLIST {
private:
	LFNODE* head, * tail;
public:
	LFLIST()
	{
		std::cout << "Testing Lock Free Synchronization List\n";
		head = new LFNODE{ std::numeric_limits<int>::min() };
		tail = new LFNODE{ std::numeric_limits<int>::max() };
		head->set_next(tail);
	}

	~LFLIST() {
		clear();
		delete head;
		delete tail;
	}

	void clear()
	{
		LFNODE* current = head->get_next();
		while (head->get_next() != tail) {
			LFNODE* temp = head->get_next();
			head->set_next(temp->get_next());
			delete temp;
		}
	}

	void find(int x, LFNODE* &pred, LFNODE* &curr)
	{
		retry:
		pred = head;
		curr = pred->get_next();
		while (true) {
			// Check if curr is marked as removed
			bool removed = false;
			while (true) {
				LFNODE* succ = curr->get_next(&removed);
				if (false == removed) break; // If curr is not removed, break the inner loop
				if (false == pred->CAS(curr, succ, false, false)) goto retry;
				lf_memory_pool[thread_id].free_node(curr); // Recycle the removed node back to the memory pool
				curr = succ;
			}
			if (curr->data >= x) break;
			pred = curr;
			curr = curr->get_next();
		}
	}

	bool Add(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			find(x, pred, curr);
			if (curr->data == x) return false; // Element already exists
			else {
				LFNODE* new_node = lf_memory_pool[thread_id].get_node(x);
				new_node->set_next(curr);
				if (true == pred->CAS(curr, new_node, false, false))
					return true; // Attempt to link the new node between pred and curr
				lf_memory_pool[thread_id].free_node(new_node); // Recycle the unused node back to the memory pool							
			}
		}
	}

	bool Remove(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			find(x, pred, curr);
			if (curr->data != x) return false; // Element already exists
			else {
				LFNODE* succ = curr->get_next();
				if (false == curr->CAS(succ, succ, false, true)) continue; // Attempt to mark the node as removed
				if (true == pred->CAS(curr, succ, false, false)) // Attempt to unlink the removed node from the list
					lf_memory_pool[thread_id].free_node(curr); // Recycle the unused node back to the memory pool
				return true;
			}
		}
	}

	bool Contains(int x)
	{
		LFNODE* n = head;
		while (n->data < x) {
			n = n->get_next();
		}
		return (n->data == x) && (false == n->get_mark()); // Check if the node exists and is not removed
	}

	void print20()
	{
		LFNODE* curr = head->get_next();
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->get_next();
			count++;
		}
		std::cout << "\n";
	}
};

class LFEBRLIST {
private:
	LFNODE* head, * tail;
public:
	LFEBRLIST()
	{
		std::cout << "Testing Lock Free Synchronization List\n";
		head = new LFNODE{ std::numeric_limits<int>::min() };
		tail = new LFNODE{ std::numeric_limits<int>::max() };
		head->set_next(tail);
	}

	~LFEBRLIST() {
		clear();
		delete head;
		delete tail;
	}

	void clear()
	{
		LFNODE* current = head->get_next();
		while (head->get_next() != tail) {
			LFNODE* temp = head->get_next();
			head->set_next(temp->get_next());
			delete temp;
		}
	}

	void find(int x, LFNODE*& pred, LFNODE*& curr)
	{
	retry:
		pred = head;
		curr = pred->get_next();
		while (true) {
			// Check if curr is marked as removed
			bool removed = false;
			while (true) {
				LFNODE* succ = curr->get_next(&removed);
				if (false == removed) break; // If curr is not removed, break the inner loop
				if (false == pred->CAS(curr, succ, false, false)) goto retry;
				ebr.freenode(curr); // Recycle the removed node back to the memory pool
				curr = succ;
			}
			if (curr->data >= x) break;
			pred = curr;
			curr = curr->get_next();
		}
	}

	bool Add(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			find(x, pred, curr);
			if (curr->data == x) return false; // Element already exists
			else {
				LFNODE* new_node = ebr.getnode(x);
				new_node->set_next(curr);
				if (true == pred->CAS(curr, new_node, false, false))
					return true; // Attempt to link the new node between pred and curr
				ebr.freenode(new_node); // Recycle the unused node back to the memory pool							
			}
		}
	}

	bool Remove(int x)
	{
		LFNODE* pred, * curr;
		while (true) {
			ebr.enter();
			find(x, pred, curr);
			if (curr->data != x) {
				ebr.leave();
				return false; // Element already exists
			} else {
				LFNODE* succ = curr->get_next();
				if (false == curr->CAS(succ, succ, false, true)) continue; // Attempt to mark the node as removed
				if (true == pred->CAS(curr, succ, false, false)) // Attempt to unlink the removed node from the list
					ebr.freenode(curr); // Recycle the unused node back to the memory pool
				ebr.leave();
				return true;
			}
		}
	}

	bool Contains(int x)
	{
		LFNODE* n = head;
		ebr.enter();
		while (n->data < x) {
			n = n->get_next();
		}
		bool found = (n->data == x) && (false == n->get_mark());
		ebr.leave();
		return (n->data == x) && (false == n->get_mark()); // Check if the node exists and is not removed
	}

	void print20()
	{
		LFNODE* curr = head->get_next();
		int count = 0;
		while (curr != tail && count < 20) {
			std::cout << curr->data << ", ";
			curr = curr->get_next();
			count++;
		}
		std::cout << "\n";
	}
};


// 싱글 쓰레드 통합 API
enum INVO_OP { ADD = 0, REMOVE = 1, CONTAINS = 2 };
class INVOCATION {
public:
	INVO_OP op;
	int value;
	INVOCATION(INVO_OP o, int v) : op(o), value(v) {}
};

typedef bool RESPONSE;

class SEQ_SET {
	std::set<int> m_set;
public:
	RESPONSE apply(INVOCATION inv) {
		switch (inv.op) {
		case ADD:
			return m_set.insert(inv.value).second;
		case REMOVE:
			return (m_set.erase(inv.value) > 0);
		case CONTAINS:
			return (m_set.find(inv.value) != m_set.end());
		default:
			return false;
		}
	}
	void clear() {
		m_set.clear();
	}
	void print20() {
		int count = 0;
		for (auto& v : m_set) {
			std::cout << v << ", ";
			if (++count >= 20) break;
		}
		std::cout << std::endl;
	}
};

class LOGNODE;

class CONSENSUS {
	LOGNODE* value{ nullptr };
public:
	LOGNODE* decide(LOGNODE* v)
	{
		CAS(&value, nullptr, v);
		return value;
	}
	void CAS(LOGNODE** addr, LOGNODE* expected, LOGNODE* update)
	{
		std::atomic_compare_exchange_strong(
			reinterpret_cast<std::atomic<LOGNODE*>*>(addr),
			&expected, update);
	}
	void clear()
	{
		value = nullptr;
	}
};

class LOGNODE {
public:
	INVOCATION m_inv;
	int	m_seq;
	LOGNODE* m_next;
	CONSENSUS decide_next;
	LOGNODE(INVOCATION inv) : m_inv(inv), m_seq(0), m_next(nullptr) {}
};

class LFU_SET {
	LOGNODE* head[MAX_THREADS];
	LOGNODE* tail;
public:
	LFU_SET() {
		tail = new LOGNODE(INVOCATION(CONTAINS, 0)); // dummy
		for (int i = 0; i < MAX_THREADS; ++i) {
			head[i] = tail;
		}
	}

	~LFU_SET()
	{
		while (nullptr != tail) {
			LOGNODE* temp = tail;
			tail = tail->m_next;
			delete temp;
		}
	}

	LOGNODE* max_head()
	{
		LOGNODE* max_node = head[0];
		for (int i = 1; i < MAX_THREADS; ++i) {
			if (max_node->m_seq < head[i]->m_seq)
				max_node = head[i];
		}
		return max_node;
	}

	RESPONSE apply(INVOCATION inv)
	{
		int i = thread_id;
		auto prefer = new LOGNODE(inv);
		while (prefer->m_seq == 0) {
			LOGNODE* before = max_head();
			LOGNODE* after = before->decide_next.decide(prefer);
			before->m_next = after;
			after->m_seq = before->m_seq + 1;
			head[i] = after;
		}

		SEQ_SET seq_set;
		LOGNODE* curr = tail->m_next;
		while (curr != prefer) {
			seq_set.apply(curr->m_inv);
			curr = curr->m_next;
		}
		if (prefer->m_seq % 1000 == 0)
			std::cout << ".";
		return seq_set.apply(inv);
	};

	void clear()
	{
		for (int i = 0; i < MAX_THREADS; ++i) {
			head[i] = tail;
		}
		LOGNODE* curr = tail->m_next;
		while (nullptr != curr) {
			LOGNODE* temp = curr;
			curr = curr->m_next;
			delete temp;
		}
		tail->m_next = nullptr;
		tail->decide_next.clear();
	}

	void print20()
	{
		SEQ_SET seq_set;
		LOGNODE* curr = tail->m_next;
		while (nullptr != curr) {
			seq_set.apply(curr->m_inv);
			curr = curr->m_next;
		}
		seq_set.print20();
	}
};


// 벤치 마킹
class STD_SET {
private:
	SEQ_SET m_set;
	//LFU_SET m_set;
	//DUMMY_MTX mtx;
public:
	STD_SET() {}

	~STD_SET() {}

	void clear()
	{
		m_set.clear();
	}

	bool Add(int x)
	{
		auto res = m_set.apply(INVOCATION(ADD, x));
		return res;
	}


	bool Remove(int x)
	{
		auto res = m_set.apply(INVOCATION(REMOVE, x));
		return res;
	}

	bool Contains(int x)
	{
		auto res = m_set.apply(INVOCATION(CONTAINS, x));
		return res;
	}

	void print20()
	{
		m_set.print20();
	}
};

constexpr int MAX_NEXTS = 9;

class SK_NODE {
	public:
	int data;
	SK_NODE* next[MAX_NEXTS] = { nullptr };
	int num_nexts;
	std::atomic<bool> removed{ false };
	std::atomic<bool> fully_linked{ false };
	std::mutex mtx;
	SK_NODE(int value) : data(value), num_nexts(1) {}
	SK_NODE(int value, int num) : data(value), num_nexts(num) {}
	void lock() { mtx.lock(); }
	void unlock() { mtx.unlock(); }
};

class C_SKLIST {
	SK_NODE* head, * tail;
	std::mutex mtx;
public:
	C_SKLIST()
	{
		std::cout << "Testing Coarse Grain Skip List\n";
		head = new SK_NODE(std::numeric_limits<int>::min());
		tail = new SK_NODE(std::numeric_limits<int>::max());
		for (int i = 0; i < MAX_NEXTS; ++i) {
			head->next[i] = tail;
		}
	}
	void clear()
	{
		SK_NODE* current = head->next[0];
		while (head->next[0] != tail) {
			SK_NODE* temp = head->next[0];
			head->next[0] = temp->next[0];
			delete temp;
		}
		for (int i = 1; i < MAX_NEXTS; ++i) {
			head->next[i] = tail;
		}
	}
	~C_SKLIST() {
		clear();
		delete head;
		delete tail;
	}

	// 탐색 결과를 반환하는 Find: pred[], succ[] 배열을 채우고 해당 레벨에서의 발견 여부 반환
	void Find(int x, SK_NODE* pred[], SK_NODE* curr[]) {
		int found_level = -1;
		SK_NODE* prev = head;

		for (int level = MAX_NEXTS - 1; level >= 0; --level) {
			if (level == MAX_NEXTS - 1)	pred[level] = head;
			else pred[level] = pred[level + 1];
			curr[level] = pred[level]->next[level];
			while (curr[level]->data < x) {
				pred[level] = curr[level];
				curr[level] = curr[level]->next[level];
			}
		}
	}

	bool Add(int x)
	{
		SK_NODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];
		mtx.lock();
		Find(x, pred, curr);
		if (curr[0]->data != x) {
			int num_nexts = 1;
			while (num_nexts < MAX_NEXTS && rand() % 2 == 0) {
				num_nexts++;
			}
			SK_NODE* new_node = new SK_NODE(x, num_nexts);
			for (int i = 0; i < num_nexts; ++i) {
				new_node->next[i] = curr[i];
				pred[i]->next[i] = new_node;
			}
			mtx.unlock();
			return true; // Element added successfully
		}
		mtx.unlock();
		return false;
	}
	bool Remove(int x)
	{
		SK_NODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];
		mtx.lock();
		Find(x, pred, curr);
		if (curr[0]->data == x) {
			for (int i = 0; i < curr[0]->num_nexts; ++i) {
				pred[i]->next[i] = curr[i]->next[i];
			}
			delete curr[0];
			mtx.unlock();
			return true; // Element removed successfully
		}
		mtx.unlock();
		return false;
	}
	bool Contains(int x)
	{
		SK_NODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];
		mtx.lock();
		Find(x, pred, curr);
		if (curr[0]->data == x) {
			mtx.unlock();
			return true; // Element found
		}
		mtx.unlock();
		return false;
	}
	void print20()
	{
		SK_NODE* curr = head->next[0];
		for (int i = 0; i < 20 && curr != tail; ++i) {
			std::cout << curr->data << ", ";
			curr = curr->next[0];
		}
		std::cout << "\n";
	}

};

class L_SKLIST {
	SK_NODE* head, * tail;
public:
	L_SKLIST()
	{
		std::cout << "Testing Lazy Skip List\n";
		head = new SK_NODE(std::numeric_limits<int>::min());
		tail = new SK_NODE(std::numeric_limits<int>::max());
		for (int i = 0; i < MAX_NEXTS; ++i) {
			head->next[i] = tail;
		}
	}
	void clear()
	{
		SK_NODE* current = head->next[0];
		while (head->next[0] != tail) {
			SK_NODE* temp = head->next[0];
			head->next[0] = temp->next[0];
			delete temp;
		}
		for (int i = 1; i < MAX_NEXTS; ++i) {
			head->next[i] = tail;
		}
	}
	~L_SKLIST() {
		clear();
		delete head;
		delete tail;
	}

	// 탐색 결과를 반환하는 Find: pred[], succ[] 배열을 채우고 해당 레벨에서의 발견 여부 반환
	int Find(int x, SK_NODE* pred[], SK_NODE* curr[]) {
		int found_level = -1;
		SK_NODE* prev = head;

		for (int level = MAX_NEXTS - 1; level >= 0; --level) {
			if (level == MAX_NEXTS - 1)	pred[level] = head;
			else pred[level] = pred[level + 1];
			curr[level] = pred[level]->next[level];
			while (curr[level]->data < x) {
				pred[level] = curr[level];
				curr[level] = curr[level]->next[level];
			}
			if ((-1 == found_level) && (curr[level]->data == x))
				found_level = level; // Element found at this level
		}
		return found_level;
	}

	bool Add(int x) { return true; }
	bool Remove(int x) { return true; }

	bool Contains(int x) { return true; }
	void print20()
	{
		SK_NODE* curr = head->next[0];
		for (int i = 0; i < 20 && curr != tail; ++i) {
			std::cout << curr->data << ", ";
			curr = curr->next[0];
		}
		std::cout << "\n";
	}

};

class SK_LFNODE {
	std::atomic_llong nexts[MAX_NEXTS];
public:
	int num_nexts;
	int data;
	long long epoch;
	SK_LFNODE(int value, int num_floor) : data(value), nexts{0}, num_nexts(num_floor) {}
	void set_next(int index, SK_LFNODE* next_node) {
		nexts[index] = reinterpret_cast<long long>(next_node);
	}
	SK_LFNODE* get_next(int index) {
		return reinterpret_cast<SK_LFNODE*>(nexts[index].load() & 0xFFFFFFFFFFFFFFFC);
	}
	SK_LFNODE* get_next(int index, bool* removed) {
		long long temp = nexts[index].load();
		*removed = (temp & 1) == 1; // Check if the least significant bit is set (marked as removed)
		return reinterpret_cast<SK_LFNODE*>(temp & 0xFFFFFFFFFFFFFFFC);
	}
	bool get_mark(int index) {
		return (nexts[index].load() & 1) == 1; // Check if the least significant bit is set (marked as removed)
	}
	bool CAS(int index, SK_LFNODE* expected_node, SK_LFNODE* new_node,
		bool expected_removed, bool new_removed)
	{
		long long expected_value = reinterpret_cast<long long>(expected_node) | (expected_removed ? 1 : 0);
		long long new_value = reinterpret_cast<long long>(new_node) | (new_removed ? 1 : 0);
		return nexts[index].compare_exchange_strong(expected_value, new_value);
	}
};

class LF_SKLIST {
	SK_LFNODE* head, * tail;
public:
	LF_SKLIST()
	{
		std::cout << "Testing Lock Free Skip List\n";
		head = new SK_LFNODE(std::numeric_limits<int>::min(), MAX_NEXTS);
		tail = new SK_LFNODE(std::numeric_limits<int>::max(), MAX_NEXTS);
		for (int i = 0; i < MAX_NEXTS; ++i) {
			head->set_next(i, tail);
		}
	}
	void clear()
	{
		SK_LFNODE* current = head->get_next(0);
		while (head->get_next(0) != tail) {
			SK_LFNODE* temp = head->get_next(0);
			head->set_next(0, temp->get_next(0));
			delete temp;
		}
		for (int i = 1; i < MAX_NEXTS; ++i) {
			head->set_next(i, tail);
		}
	}
	~LF_SKLIST() {
		clear();
		delete head;
		delete tail;
	}

	bool Find(int x, SK_LFNODE* pred[], SK_LFNODE* currs[]) {
		SK_LFNODE* prev = head;
		retry:
		for (int level = MAX_NEXTS - 1; level >= 0; --level) {
			if (level == MAX_NEXTS - 1)	pred[level] = head;
			else pred[level] = pred[level + 1];

			currs[level] = pred[level]->get_next(level);

			while (true) {
				bool removed = false;
				SK_LFNODE* succ = currs[level]->get_next(level, &removed); // Ensure visibility of the next pointer before checking data
				while (removed) {
					if (false == pred[level]->CAS(level, currs[level], succ, false, false))
						goto retry; // Retry if CAS fails
					currs[level] = succ;
					SK_LFNODE* succ = currs[level]->get_next(level, &removed); // Ensure visibility of the next pointer before checking data
				}

				if (currs[level]->data < x) {
					pred[level] = currs[level];
					currs[level] = succ;
				}
				else break;
			}
		}
		return currs[0]->data == x;
	}

	bool Add(int x)
	{
		SK_LFNODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];
		int num_nexts = 1;
		while (num_nexts < MAX_NEXTS && rng() % 2 == 0) {
			num_nexts++;
		}

		while (true) {
			bool found = Find(x, pred, curr);
			if (true == found) return false;

			SK_LFNODE* new_node = new SK_LFNODE(x, num_nexts);
			for (int i = 0; i < num_nexts; ++i)
				new_node->set_next(i, curr[i]);

			if (false == pred[0]->CAS(0, curr[0], new_node, false, false))
				continue;

			for (int i = 1; i < num_nexts; ++i) {
				while (true) {
					if (false == pred[i]->CAS(i, curr[i], new_node, false, false)) {
						Find(x, pred, curr);
					}
					else break;
				}
			}
			return true;
		}
	}
	bool Remove(int x)
	{
		SK_LFNODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];

		bool found = Find(x, pred, curr);
		if (false == found) return false; // Element not found

		SK_LFNODE* victim = curr[0];

		for (int i = victim->num_nexts - 1; i >= 1; --i) {
			while (true) {
				bool removed = false;
				SK_LFNODE* succ = victim->get_next(i, &removed);
				if (true == removed) break; // Already marked as removed at this level
				if (true == victim->CAS(i, succ, succ, false, true))
					break; // Mark the node as removed at this level
				Find(x, pred, curr); // Retry if CAS fails, to ensure visibility of the next pointer before checking data
			}
		}

		while (true) {
			bool removed = false;
			SK_LFNODE* succ = victim->get_next(0, &removed);
			if (true == removed) return false; // Already marked as removed at the lowest level
			if (true == victim->CAS(0, succ, succ, false, true)) {
				Find(x, pred, curr);
				return true;
			} // Mark the node as removed at the lowest level
		}
	}

	bool Contains(int x)
	{
		SK_LFNODE* pred[MAX_NEXTS], * curr[MAX_NEXTS];
		bool found = Find(x, pred, curr);
		return (true == found)
			&& (curr[0]->get_mark(0) == false)
			&& (pred[0]->get_next(0) == curr[0]);
	}
	void print20()
	{
		SK_LFNODE* curr = head->get_next(0);
		for (int i = 0; i < 20 && curr != tail; ++i) {
			std::cout << curr->data << ", ";
			curr = curr->get_next(0);
		}
		std::cout << "\n";
	}

};

LF_SKLIST my_set;

#include <array>

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
	std::array <int, RANGE> survive = {};
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
	for (int i = 0; i < RANGE; ++i) {
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
			if (my_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == my_set.Contains(i)) {
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
		int op = rng() % 3;
		switch (op) {
		case 0: {
			int v = rng() % RANGE;
			history[th_id].emplace_back(0, v, my_set.Add(v));
			break;
		}
		case 1: {
			int v = rng() % RANGE;
			history[th_id].emplace_back(1, v, my_set.Remove(v));
			break;
		}
		case 2: {
			int v = rng() % RANGE;
			history[th_id].emplace_back(2, v, my_set.Contains(v));
			break;
		}
		}
	}
	memory_pool[thread_id].recycle_nodes();
}
void benchmark(int num_threads, int tid)
{
	thread_id = tid;
	const int LOOP = NUM_TEST / num_threads;
	for (int i = 0; i < LOOP; ++i) {
		int value = rng() % 1000;
		int op = rng() % 3;
		switch (op) {
		case 0:
			my_set.Add(value);
			break;
		case 1:
			my_set.Remove(value);
			break;
		case 2:
			my_set.Contains(value);
			break;
		}
	}
	memory_pool[thread_id].recycle_nodes(); // Recycle nodes back to the memory pool after the benchmark
}

int main()
{
	using namespace std::chrono;

	std::cout << "Strting Error Check.\n";
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
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
		my_set.print20();
		std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";
		check_history(num_threads);
		my_set.clear();
	}

	for (auto& h : history) h.clear();
	std::cout << "Strting Performance Check.\n";
	for (int num_threads = 1; num_threads <= MAX_THREADS; num_threads *= 2) {
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
		my_set.print20();
		std::cout << "Threads: " << num_threads << ", Time: " << exec_ms << " seconds\n";
		my_set.clear();
		//std::string temp;
		//std::getline(std::cin, temp);
	}
}