#pragma once
#include <string>
#include <vector>

// If you prefer your custom HashTable, define USE_CUSTOM_HASHTABLE in the build (or uncomment below).
// #define USE_CUSTOM_HASHTABLE

#ifdef USE_CUSTOM_HASHTABLE
#include "HashTable.h" // expects HashTable(size_t) ctor and methods insert(name, Order*) and search(name)->vector<Order*>
#else
#include <unordered_map>
#endif

#include "../models/Order.h"

// Adapter with minimal API used by the project
class OrderIndex {
public:
	OrderIndex(size_t hint = 1024) {
#ifdef USE_CUSTOM_HASHTABLE
		table_ = new HashTable(hint);
#else
		(void)hint;
#endif
	}
	~OrderIndex() {
#ifdef USE_CUSTOM_HASHTABLE
		delete table_;
#endif
	}

	// insert an Order* under customer name
	void insert(const std::string& name, Order* o) {
#ifdef USE_CUSTOM_HASHTABLE
		table_->insert(name, o);
#else
		map_[name].push_back(o);
#endif
	}

	// return vector<Order*> (empty if none)
	std::vector<Order*> search(const std::string& name) const {
#ifdef USE_CUSTOM_HASHTABLE
		return table_->search(name);
#else
		auto it = map_.find(name);
		if (it == map_.end()) return {};
		return it->second;
#endif
	}

	// erase all entries for name
	void erase(const std::string& name) {
#ifdef USE_CUSTOM_HASHTABLE
		// custom HashTable currently has no erase; if you add erase(name) implement it there.
		// Fall back: no-op
#else
		map_.erase(name);
#endif
	}

	// clear all
	void clear() {
#ifdef USE_CUSTOM_HASHTABLE
		// no-op unless you add clear() to HashTable
#else
		map_.clear();
#endif
	}

private:
#ifdef USE_CUSTOM_HASHTABLE
	HashTable* table_;
#else
	std::unordered_map<std::string, std::vector<Order*>> map_;
#endif
};
