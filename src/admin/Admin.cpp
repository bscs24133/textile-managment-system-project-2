#include "Admin.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <algorithm>
#include "../controllers/Auth.h" // for getAllCustomers

AdminController::AdminController() {
	// bootstrap a default admin credential (username: admin, password: admin)
	adminCreds_["admin"] = hashPassword("admin");
}

std::string AdminController::hashPassword(const std::string &pw) const {
	// placeholder hashing - NOT secure; replace with proper hash in production
	return std::to_string(std::hash<std::string>{}(pw));
}

bool AdminController::authenticate(const std::string &username, const std::string &password) const {
	auto it = adminCreds_.find(username);
	if (it == adminCreds_.end()) return false;
	return it->second == hashPassword(password);
}

time_t AdminController::predictDeliveryTime(int shirtQuantity, time_t startTime) const {
	// rule: 1 shirt = 3 minutes
	long long seconds = static_cast<long long>(shirtQuantity) * 3LL * 60LL;
	return startTime + seconds;
}

std::string AdminController::timeToString(time_t t) const {
	if (t == 0) return "N/A";
	std::tm tmBuf;
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tmBuf, &t);
#else
	localtime_r(&t, &tmBuf);
#endif
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmBuf);
	return std::string(buf);
}

// stage rank: Cutting=1, Stitching=2, Finishing=3, Packed=4 (higher rank = later stage)
static int stageRankFromString(const std::string &s) {
	if (s == "Cutting") return 1;
	if (s == "Stitching") return 2;
	if (s == "Finishing") return 3;
	if (s == "Packed") return 4;
	// fallback: Received -> 0
	if (s == "Received") return 0;
	return 0;
}

long long AdminController::computePriority(const AdminOrder &o) const {
	// weight stage heavily so later stages outrank earlier ones.
	// Priority = stageRank * 1e12 - secondsUntilDelivery
	long long rank = stageRankFromString(o.orderStatus);
	time_t now = std::time(nullptr);
	long long secondsUntilDelivery = static_cast<long long>(o.predictedDeliveryTime - now);
	// Use a big multiplier for stage
	const long long STAGE_WEIGHT = 1000000000000LL; // 1e12
	return rank * STAGE_WEIGHT - secondsUntilDelivery;
}

void AdminController::addOrderRecord(int orderID, int customerID, const std::string &fullName, int shirtQuantity, const std::string &orderStatus, time_t orderStartTime) {
	AdminOrder rec;
	rec.orderID = orderID;
	rec.customerID = customerID;
	rec.fullName = fullName;
	rec.shirtQuantity = shirtQuantity;
	rec.orderStatus = orderStatus;
	rec.orderStartTime = orderStartTime;
	rec.predictedDeliveryTime = predictDeliveryTime(shirtQuantity, orderStartTime);

	// insert or overwrite
	auto it = orderTable_.find(orderID);
	if (it == orderTable_.end()) {
		orderTable_[orderID] = rec;
		orderOrder_.push_back(orderID);
		customerOrders_[customerID].push_back(orderID);
	} else {
		orderTable_[orderID] = rec;
		// ensure customerOrders_ consistency: if changed customerID, update mapping (simpler: append if missing)
		auto &vec = customerOrders_[customerID];
		if (std::find(vec.begin(), vec.end(), orderID) == vec.end()) vec.push_back(orderID);
	}
}

bool AdminController::updateOrderStatusByOrderID(int orderID, const std::string &newStatus) {
	auto it = orderTable_.find(orderID);
	if (it == orderTable_.end()) return false;
	it->second.orderStatus = newStatus;
	// recompute predictedDeliveryTime only if you want; keep existing predictedDeliveryTime
	return true;
}

const AdminOrder* AdminController::getOrder(int orderID) const {
	auto it = orderTable_.find(orderID);
	if (it == orderTable_.end()) return nullptr;
	return &it->second;
}

std::vector<AdminOrder> AdminController::listAllOrdersByPriority() const {
	std::vector<AdminOrder> res;
	res.reserve(orderTable_.size());
	for (int id : orderOrder_) {
		auto it = orderTable_.find(id);
		if (it != orderTable_.end()) res.push_back(it->second);
	}
	std::sort(res.begin(), res.end(), [this](const AdminOrder &a, const AdminOrder &b) {
		return computePriority(a) > computePriority(b); // higher first
	});
	return res;
}

std::vector<AdminOrder> AdminController::listCustomerOrdersByPriority(int customerID) const {
	std::vector<AdminOrder> res;
	auto it = customerOrders_.find(customerID);
	if (it == customerOrders_.end()) return res;
	for (int oid : it->second) {
		auto it2 = orderTable_.find(oid);
		if (it2 != orderTable_.end()) res.push_back(it2->second);
	}
	std::sort(res.begin(), res.end(), [this](const AdminOrder &a, const AdminOrder &b) {
		return computePriority(a) > computePriority(b);
	});
	return res;
}

void AdminController::runMenu(const AuthController &auth) {
	std::cout << "Admin dashboard opened.\n";
	while (true) {
		std::cout << "\nAdmin Dashboard\n1) View All Customers (Order Summary)\n2) View All Orders (by priority)\n3) View Customer Details / Orders\n4) Update Order Status by OrderID\n5) Logout\nChoose: ";
		int opt; if (!(std::cin >> opt)) break;
		if (opt == 1) {
			// List every customer from customers.data (even if they have no orders)
			auto customers = auth.getAllCustomers();
			std::cout << "\n[Customer ID]  Full Name               Order Count  Latest Status\n";
			std::cout << "---------------------------------------------------------------\n";
			for (const auto &cust : customers) {
				int cid = cust.id;
				std::string name = cust.fullName;
				auto it = customerOrders_.find(cid);
				int count = (it != customerOrders_.end()) ? (int)it->second.size() : 0;
				std::string latest = "No Orders";
				if (count > 0) {
					int lastOid = it->second.back();
					auto oit = orderTable_.find(lastOid);
					if (oit != orderTable_.end()) latest = oit->second.orderStatus;
				}
				std::cout << std::setw(12) << cid << "  " << std::left << std::setw(20) << name << "  " << std::right << std::setw(10) << count << "  " << latest << '\n';
			}
		} else if (opt == 2) {
			auto list = listAllOrdersByPriority();
			std::cout << "\n[OrderID] [CustomerID]  Full Name           Stage        PredictedDelivery\n";
			std::cout << "---------------------------------------------------------------------\n";
			for (const auto &o : list) {
				std::cout << std::setw(7) << o.orderID << "  " << std::setw(10) << o.customerID << "  " << std::left << std::setw(18) << o.fullName << "  " << std::setw(10) << o.orderStatus << "  " << timeToString(o.predictedDeliveryTime) << '\n';
			}
		} else if (opt == 3) {
			std::cout << "Enter Customer ID: ";
			int cid; std::cin >> cid;
			auto list = listCustomerOrdersByPriority(cid);
			if (list.empty()) { std::cout << "No orders for this customer.\n"; continue; }
			std::cout << "\n[OrderID]  Stage        Quantity  Start Time           PredictedDelivery\n";
			std::cout << "---------------------------------------------------------------------\n";
			for (const auto &o : list) {
				std::cout << std::setw(7) << o.orderID << "  " << std::left << std::setw(10) << o.orderStatus << "  " << std::right << std::setw(8) << o.shirtQuantity << "  " << std::setw(20) << timeToString(o.orderStartTime) << "  " << timeToString(o.predictedDeliveryTime) << '\n';
			}
		} else if (opt == 4) {
			std::cout << "Enter Order ID: ";
			int oid; std::cin >> oid;
			const AdminOrder* ao = getOrder(oid);
			if (!ao) { std::cout << "Order not found\n"; continue; }
			std::cout << "Current status: " << ao->orderStatus << '\n';
			std::cout << "New status (Cutting / Stitching / Finishing / Packed): ";
			std::string ns; std::cin >> ns;
			if (updateOrderStatusByOrderID(oid, ns)) std::cout << "Status updated.\n";
			else std::cout << "Failed to update.\n";
		} else break;
	}
}
