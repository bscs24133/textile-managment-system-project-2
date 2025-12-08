#pragma once
#include "../models/Order.h"
#include <vector>
#include <string>

class HashTable {
public:
	HashTable(size_t sz = 101);
	void insert(const std::string& name, Order* orderPtr);
	std::vector<Order*> search(const std::string& name) const;

private:
	size_t size_;
	std::vector<std::vector<Order*>> table_;
	size_t hashFunction(const std::string& name) const;
};
