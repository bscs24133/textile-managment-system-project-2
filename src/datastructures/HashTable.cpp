#include "HashTable.h"
#include <functional>
#include <algorithm>

HashTable::HashTable(size_t sz) : size_(sz), table_(sz) {}

size_t HashTable::hashFunction(const std::string& name) const {
	return std::hash<std::string>{}(name) % size_;
}

void HashTable::insert(const std::string& name, Order* orderPtr) {
	auto idx = hashFunction(name);
	table_[idx].push_back(orderPtr);
}

std::vector<Order*> HashTable::search(const std::string& name) const {
	std::vector<Order*> res;
	auto idx = hashFunction(name);
	for (auto o : table_[idx]) {
		if (o->customerName == name) res.push_back(o);
	}
	return res;
}
