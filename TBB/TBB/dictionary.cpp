#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_map.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/parallel_for.h>
#include <tbb/concurrent_queue.h>
#include <vector>
#include <string> 
#include <fstream>
#include <iostream>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <locale>

using namespace tbb;
using namespace std;
using namespace chrono;

// Structure that defines hashing and comparison operations for user's type. 
typedef concurrent_unordered_map<string, int> StringTable;

int main()
{
	vector<string> Data;
	vector<string> Original;
	StringTable table;
	locale loc;

	cout << "Loading!\n";
	ifstream openFile("TextBook.txt");
	if (openFile.is_open()) {
		string word;
		while (false == openFile.eof()) {
			openFile >> word;
			if (isalpha(word[0], loc))
				Original.push_back(word);
		}
		openFile.close();
	}

	cout << "Loaded Total " << Original.size() << " Words.\n";

	cout << "Duplicating!\n";

	for (int i = 0; i < 1000; ++i)
		for (auto& word : Original) Data.push_back(word);

	cout << "Counting!\n";

	// 싱글 쓰레드 속도 측정
	{
		auto start = high_resolution_clock::now();
		unordered_map<string, int> table;
		for (auto& w : Data)
			table[w]++;

		auto du = high_resolution_clock::now() - start;

		int count = 0;
		for (auto& w : table) {
			if (count++ > 10) break;
			cout << "[" << w.first << ", " << w.second << "], ";
		}

		cout << "\nSingle Thread Time : " << duration_cast<milliseconds>(du).count() << endl << endl;
	}

	{
		int num_th = thread::hardware_concurrency();
		vector <thread> threads;
		auto start = high_resolution_clock::now();
		unordered_map<string, int> table;
		mutex tl;
		for (int i = 0; i < num_th; ++i)
			threads.emplace_back([&tl, &Data, &table, i, &num_th]() {
			const int task = Data.size() / num_th;
			const int data_size = Data.size();
			for (int j = 0; j < task; ++j) {
				int loc = j + i * task;
				if (loc >= data_size) break;
				tl.lock();
				table[Data[loc]]++;
				tl.unlock();
			}
				});
		for (auto& th : threads) th.join();

		auto du = high_resolution_clock::now() - start;

		int count = 0;
		for (auto& w : table) {
			if (count++ > 10) break;
			cout << "[" << w.first << ", " << w.second << "], ";
		}

		cout << "\nMulti Thread Time : " << duration_cast<milliseconds>(du).count() << endl << endl;
	}

	{
		auto start = high_resolution_clock::now();
		concurrent_unordered_map<string, atomic_int> table;
		const int DATA_SIZE = Data.size();
		parallel_for(0, DATA_SIZE, [&Data, &table](int i) {
			table[Data[i]]++;
			});

		auto du = high_resolution_clock::now() - start;

		int count = 0;
		for (auto& w : table) {
			if (count++ > 10) break;
			cout << "[" << w.first << ", " << w.second << "], ";
		}

		cout << "\nTBB Time : " << duration_cast<milliseconds>(du).count() << endl <<endl;
	}

	{
		auto start = high_resolution_clock::now();
		concurrent_hash_map<string, atomic_int> table;
		const int DATA_SIZE = Data.size();
		parallel_for(0, DATA_SIZE, [&Data, &table](int i) {
			concurrent_hash_map<string, atomic_int> ::accessor p;
			table.insert(p, Data[i]);
			p->second++;
			});

		auto du = high_resolution_clock::now() - start;

		int count = 0;
		for (auto& w : table) {
			if (count++ > 10) break;
			cout << "[" << w.first << ", " << w.second << "], ";
		}

		cout << "\nTBB concurrent_hash_map Time : " << duration_cast<milliseconds>(du).count() << endl << endl;
	}

	{
		auto start = high_resolution_clock::now();
		concurrent_map<string, atomic_int> table;
		const int DATA_SIZE = static_cast<int>(Data.size());
		parallel_for(0, DATA_SIZE, [&Data, &table](int i) {
			table[Data[i]]++;
			});

		auto du = high_resolution_clock::now() - start;

		int count = 0;
		for (auto& w : table) {
			if (count++ > 10) break;
			cout << "[" << w.first << ", " << w.second << "], ";
		}

		cout << "\nTBB concurrent_map Time : " << duration_cast<milliseconds>(du).count() << endl;
	}
}
