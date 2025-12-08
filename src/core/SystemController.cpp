#include "SystemController.h"
#include <iostream>
#include <limits>

SystemController::SystemController() : idIndex(3), nameIndex(101) {}
SystemController::~SystemController() {
	for (auto p : allOrders) delete p;
}

void SystemController::addOrder() {
	int id, qty;
	std::string customer, product, date;
	std::cout << "Enter Order ID: "; std::cin >> id; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::cout << "Enter Customer Name: "; std::getline(std::cin, customer);
	std::cout << "Enter Product Type: "; std::getline(std::cin, product);
	std::cout << "Enter Quantity: "; std::cin >> qty; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::cout << "Enter Date of Order (YYYY-MM-DD): "; std::getline(std::cin, date);

	Order* o = new Order(id, customer, product, qty, date);
	// insert into structures
	idIndex.insert(id, o);
	nameIndex.insert(customer, o);
	stageQueue.enqueue(o);
	allOrders.push_back(o);

	std::cout << "Order added and enqueued for production.\n";
}

void SystemController::searchByID() {
	int id;
	std::cout << "Enter Order ID to search: "; std::cin >> id;
	Order* o = idIndex.search(id);
	if (!o) std::cout << "Order not found.\n";
	else {
		o->display();
	}
}

void SystemController::searchByCustomer() {
	std::string customer;
	std::cout << "Enter Customer Name to search: "; std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	std::getline(std::cin, customer);
	auto res = nameIndex.search(customer);
	if (res.empty()) {
		std::cout << "No orders found for customer: " << customer << "\n";
		return;
	}
	for (auto o : res) {
		o->display();
		std::cout << "----\n";
	}
}

void SystemController::advanceStage() {
	Order* o = stageQueue.dequeue();
	if (!o) {
		std::cout << "Production queue is empty.\n";
		return;
	}
	// advance stage
	stageQueue.advanceStageForOrder(o);
	// if now final (Finished) do not requeue, else requeue
	if (o->productionStage == "Finished") {
		std::cout << "Order " << o->orderID << " has completed production and is finished.\n";
	} else {
		stageQueue.enqueue(o);
		std::cout << "Order " << o->orderID << " advanced to stage: " << o->productionStage << " and requeued.\n";
	}
}

void SystemController::viewAllOrders() const {
	if (allOrders.empty()) {
		std::cout << "No orders in system.\n";
		return;
	}
	for (auto o : allOrders) {
		o->display();
		std::cout << "----\n";
	}
}

void SystemController::viewProductionQueue() const {
	stageQueue.displayQueueStages();
}
