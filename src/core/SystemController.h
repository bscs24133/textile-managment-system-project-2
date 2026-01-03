#pragma once
#include "../datastructures/BTree.h"
#include "../datastructures/HashTable.h"
#include "../datastructures/ProductionQueue.h"
#include "../models/Order.h"
#include <vector>

class SystemController {
public:
	SystemController();
	~SystemController();

	void addOrder();
	void searchByID();
	void searchByCustomer();
	void advanceStage();
	void viewAllOrders() const;
	void viewProductionQueue() const;

private:

	BTree idIndex;
	HashTable nameIndex;
	ProductionQueue stageQueue;
	std::vector<Order*> allOrders;
};
