#pragma once
#include <queue> // added: required for std::priority_queue
#include <vector>
#include <utility>

template<typename T>
class PriorityQueue {
public:
	// pair: <priority, T*>
	using Item = std::pair<int, T*>;

	void push(int priority, T* value) {
		pq.emplace(priority, value);
	}
	bool empty() const { return pq.empty(); }
	Item top() const { return pq.top(); }
	void pop() { pq.pop(); }
private:
	struct Cmp {
		bool operator()(const Item &a, const Item &b) const {
			return a.first < b.first; // larger priority first
		}
	};
	std::priority_queue<Item, std::vector<Item>, Cmp> pq;
};
