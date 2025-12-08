#include "Order.h"
#include <iostream>

Order::Order(int id, const std::string& customer, const std::string& product, int qty, const std::string& date)
	: orderID(id), customerName(customer), productType(product), quantity(qty), productionStage("Cutting"), dateOfOrder(date) {}

void Order::display() const {
	std::cout << "Order ID: " << orderID << "\n"
	          << "Customer: " << customerName << "\n"
	          << "Product: " << productType << "\n"
	          << "Quantity: " << quantity << "\n"
	          << "Stage: " << productionStage << "\n"
	          << "Date: " << dateOfOrder << "\n";
}
