#pragma once
#include "../models/Order.h"
#include <queue>
#include <vector>

class ProductionQueue {
public:
	void enqueue(Order* o);
	Order* dequeue();
	void advanceStageForOrder(Order* o);
	void displayQueueStages() const;
	bool empty() const;
};
