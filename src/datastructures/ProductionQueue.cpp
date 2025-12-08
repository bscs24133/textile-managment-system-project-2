#include "ProductionQueue.h"
#include <iostream>

static std::string nextStage(const std::string& stage) {
	if (stage == "Cutting") return "Stitching";
	if (stage == "Stitching") return "Finishing";
	if (stage == "Finishing") return "Packed";
	return ""; // already final or unknown
}

static bool isFinalStage(const std::string& stage) {
	return stage == "Packed" || stage == "Finished";
}

struct QueueWrapper {
	std::queue<Order*> q;
};

static QueueWrapper& getQueue() {
	static QueueWrapper instance;
	return instance;
}

void ProductionQueue::enqueue(Order* o) {
	getQueue().q.push(o);
}

Order* ProductionQueue::dequeue() {
	auto &qq = getQueue().q;
	if (qq.empty()) return nullptr;
	Order* o = qq.front();
	qq.pop();
	return o;
}

void ProductionQueue::advanceStageForOrder(Order* o) {
	if (!o) return;
	auto ns = nextStage(o->productionStage);
	if (ns.empty()) {
		// if currently Packed, mark Finished (final)
		if (o->productionStage == "Packed") o->productionStage = "Finished";
		return;
	}
	o->productionStage = ns;
}

void ProductionQueue::displayQueueStages() const {
	auto q = getQueue().q; // copy
	if (q.empty()) {
		std::cout << "Production queue is empty.\n";
		return;
	}
	std::cout << "Production Queue:\n";
	while (!q.empty()) {
		Order* o = q.front(); q.pop();
		std::cout << "ID: " << o->orderID << " | Customer: " << o->customerName
		          << " | Stage: " << o->productionStage << "\n";
	}
}

bool ProductionQueue::empty() const {
	return getQueue().q.empty();
}
