#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <ctime>

struct AdminOrder {
	int orderID;
	int customerID;
	std::string fullName;
	int shirtQuantity;
	std::string orderStatus; // Cutting, Stitching, Finishing, Packed, etc.
	time_t orderStartTime;
	time_t predictedDeliveryTime;
};

class AuthController; // forward declaration

class AdminController {
public:
	AdminController();

	// Authenticate admin credentials (returns true on success)
	bool authenticate(const std::string &username, const std::string &password) const;

	// Run the admin menu (assumes caller already authenticated) — needs AuthController to list all customers
	void runMenu(const AuthController &auth);

	// programmatic API used by main when a customer places an order
	// caller provides the orderID
	void addOrderRecord(int orderID, int customerID, const std::string &fullName, int shirtQuantity, const std::string &orderStatus, time_t orderStartTime);

	// update order status by orderID
	bool updateOrderStatusByOrderID(int orderID, const std::string &newStatus);

	// list all orders sorted by priority (high -> low)
	std::vector<AdminOrder> listAllOrdersByPriority() const;

	// list orders for a given customer sorted by priority
	std::vector<AdminOrder> listCustomerOrdersByPriority(int customerID) const;

	// query single order
	const AdminOrder* getOrder(int orderID) const;

private:
	std::unordered_map<std::string, std::string> adminCreds_; // username -> passHash

	// main order storage
	std::unordered_map<int, AdminOrder> orderTable_; // orderID -> AdminOrder
	std::vector<int> orderOrder_; // insertion order of orderIDs

	// map customerID -> list of orderIDs
	std::unordered_map<int, std::vector<int>> customerOrders_;

	// helpers
	std::string hashPassword(const std::string &pw) const;
	std::string timeToString(time_t t) const;
	time_t predictDeliveryTime(int shirtQuantity, time_t startTime) const;

	// priority helper: larger value -> higher priority
	long long computePriority(const AdminOrder &o) const;
};
