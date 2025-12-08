#pragma once
#include <string>
class Order {
public:
	int orderID;
	std::string customerName;
	std::string productType;
	int quantity;
	std::string productionStage;
	std::string dateOfOrder;

	Order(int id, const std::string& customer, const std::string& product, int qty, const std::string& date);
	void display() const;
};
