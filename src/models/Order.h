#pragma once
#include <string>

struct Order {
	int orderID;
	std::string customerName;
	std::string productType;
	int quantity;
	std::string productionStage;
	std::string dateOfOrder;

	Order() : orderID(0), quantity(0) {}
	Order(int id, const std::string &cust, const std::string &prod, int qty, const std::string &date)
		: orderID(id), customerName(cust), productType(prod), quantity(qty), productionStage("Received"), dateOfOrder(date) {}
};
