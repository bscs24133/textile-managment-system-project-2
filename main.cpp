// To build and run (make sure to include admin and other cpp files):
//   g++ -std=c++17 -I. main.cpp src/controllers/Auth.cpp src/admin/Admin.cpp src/datastructures/BTree.cpp src/datastructures/HashTable.cpp -o main
// or compile all src .cpp automatically:
//   g++ -std=c++17 -I. main.cpp $(find src -name '*.cpp' -print) -o main
// Then run:
//   ./main
// If you still see the old menu, ensure you rebuilt and you're executing the binary you just built:
//   which ./main && ls -l ./main && ./main

#include <iostream>
#include <string>
#include <ctime>
#include <sstream>
#include <vector>
#include <algorithm> // added to fix compilation errors using std::find / algorithms
#include "src/controllers/Auth.h"
#include "src/models/Order.h"
#include "src/datastructures/BTree.h"
#include "src/datastructures/PriorityQueue.h"
#include "src/admin/Admin.h"
#include <iomanip>
using namespace std;

// simple serializer/deserializer for Order*
std::string orderSerializer(Order* o) {
	if (!o) return "";
	// orderID|customerName|productType|quantity|productionStage|dateOfOrder
	std::ostringstream oss;
	oss << o->orderID << '|' << o->customerName << '|' << o->productType << '|' << o->quantity << '|' << o->productionStage << '|' << o->dateOfOrder;
	return oss.str();
}
Order* orderDeserializer(const std::string &line) {
	std::vector<std::string> toks;
	std::string cur;
	for (size_t i = 0; i < line.size(); ++i) {
		char c = line[i];
		if (c == '|') { toks.push_back(cur); cur.clear(); }
		else cur.push_back(c);
	}
	toks.push_back(cur);
	if (toks.size() < 6) return nullptr;
	int id = std::stoi(toks[0]);
	Order* o = new Order(id, toks[1], toks[2], std::stoi(toks[3]), toks[5]);
	o->productionStage = toks[4];
	return o;
}

// new: predict delivery date string (YYYY-MM-DD) based on quantity
static std::string predictDeliveryDate(int quantity) {
	// simple heuristic:
	// base production time = 3 days
	// add 1 day for each full 10 items beyond the first 10
	int baseDays = 3;
	int extraPerTen = 1;
	int extra = 0;
	if (quantity > 10) extra = (quantity - 1) / 10 * extraPerTen;
	int totalDays = baseDays + extra;

	std::time_t now = std::time(nullptr);
	std::time_t then = now + totalDays * 24 * 60 * 60;
	std::tm tmBuf;
#if defined(_WIN32) || defined(_WIN64)
	localtime_s(&tmBuf, &then);
#else
	localtime_r(&then, &tmBuf);
#endif
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tmBuf);
	return std::string(buf);
}

// parse date string "YYYY-MM-DD" to time_t (midnight local time). returns 0 on failure.
static time_t parseDate(const std::string &dateStr) {
	if (dateStr.size() < 10) return 0;
	int y = 0, m = 0, d = 0;
	char c1, c2;
	std::istringstream iss(dateStr);
	iss >> y >> c1 >> m >> c2 >> d;
	if (iss.fail() || c1 != '-' || c2 != '-') return 0;
	std::tm tmv = {};
	tmv.tm_year = y - 1900;
	tmv.tm_mon = m - 1;
	tmv.tm_mday = d;
	tmv.tm_hour = 0;
	tmv.tm_min = 0;
	tmv.tm_sec = 0;
	return std::mktime(&tmv);
}

// new: format time_t as "YYYY-MM-DD" (returns "N/A" for 0)
static std::string formatDate(time_t t) {
	if (t == 0) return std::string("N/A");
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

int main() {

	AuthController auth;
	AdminController admin;
	// create BTree<Order*> with degree 3
	BTree<Order*> orderTree(3, orderSerializer, orderDeserializer, "orders.data", "orders.index", "orders.meta");
	PriorityQueue<Order> pq; // could be PriorityQueue<Order*>, using Order pointer in push

	// Register persisted orders (loaded into BTree) into admin controller so Admin can see existing users/orders.
	{
		auto persisted = orderTree.getPersistedItems();
		for (auto po : persisted) {
			if (!po) continue;
			// lookup customer record by username to obtain customerID and fullName
			Customer* c = auth.getCustomerByUsername(po->customerName);
			if (!c) continue;
			time_t startTime = parseDate(po->dateOfOrder);
			// register with admin using orderID from the persisted order
			admin.addOrderRecord(po->orderID, c->id, c->fullName, po->quantity, po->productionStage, startTime);
			delete c;
		}
	}

	while (true) {
		std::cout << "\n1) Signup\n2) Login\n3) Admin Login\n4) Exit\nChoose: ";
		int opt; if (!(std::cin >> opt)) break;
		if (opt == 1) {
			std::string user, pass, name;
			std::cout << "username: "; std::cin >> user;
			std::cout << "password: "; std::cin >> pass;
			std::cout << "full name: "; std::ws(std::cin); std::getline(std::cin, name);
			Customer* c = auth.signup(user, pass, name);
			if (c) { std::cout << "Signed up. id=" << c->id << "\n"; delete c; }
			else std::cout << "Signup failed (maybe user exists)\n";
		} else if (opt == 2) {
			std::string user, pass;
			std::cout << "username: "; std::cin >> user;
			std::cout << "password: "; std::cin >> pass;
			Customer* cur = auth.login(user, pass);
			if (!cur) { std::cout << "Login failed\n"; continue; }
			std::cout << "Welcome, " << cur->fullName << "\n";
			// simple logged-in menu
			while (true) {
				std::cout << "\n1) Place order\n2) Check order status by ID\n3) Logout\n4) View my orders (prioritized)\nChoose: ";
				int o; if (!(std::cin >> o)) { o = 3; }
				if (o == 1) {
					int nextId = 1;
					// derive next id from persisted orders count + 1
					auto persisted = orderTree.getPersistedItems();
					nextId = (int)persisted.size() + 1;
					std::string prod; int qty;
					std::cout << "product type: "; std::cin >> prod;
					std::cout << "quantity: "; std::cin >> qty;
					// date string simple
					std::time_t t = std::time(nullptr);
					char buf[64]; std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
					Order* oitem = new Order(nextId, cur->username, prod, qty, buf);

					// compute and attach predicted delivery date (ETA) to productionStage so it's persisted
					std::string eta = predictDeliveryDate(qty);
					oitem->productionStage = "Received (ETA: " + eta + ")";

					orderTree.insert(oitem->orderID, oitem);
					// register in admin controller with orderID
					time_t startTime = std::time(nullptr);
					admin.addOrderRecord(oitem->orderID, cur->id, cur->fullName, qty, oitem->productionStage, startTime);
					std::cout << "Order placed with ID: " << oitem->orderID << "\n";
					std::cout << "Predicted delivery date: " << eta << "\n";
				} else if (o == 2) {
					int id; std::cout << "Order ID: "; std::cin >> id;
					Order* found = orderTree.search(id);
					if (!found) std::cout << "Order not found\n";
					else {
						std::cout << "Order " << found->orderID << " by " << found->customerName << "\n";
						std::cout << "Product: " << found->productType << " Qty: " << found->quantity << "\n";
						std::cout << "Stage: " << found->productionStage << "\n";
						std::cout << "Date: " << found->dateOfOrder << "\n";
					}
				} else if (o == 4) {
					// show customer's orders prioritized
					auto list = admin.listCustomerOrdersByPriority(cur->id);
					if (list.empty()) { std::cout << "You have no orders.\n"; }
					else {
						std::cout << "\n[OrderID]  Stage        Quantity  PredictedDelivery\n";
						std::cout << "--------------------------------------------------\n";
						for (const auto &a : list) {
							std::cout << std::setw(7) << a.orderID << "  " << std::left << std::setw(10) << a.orderStatus << "  " << std::right << std::setw(8) << a.shirtQuantity << "  " << std::setw(12) << formatDate(a.predictedDeliveryTime) << '\n';
						}
					}
				} else break;
			}
			delete cur;
		} else if (opt == 3) {
			// prompt for admin credentials here and run menu only if authenticated
			std::string adminUser, adminPass;
			std::cout << "Admin username: "; std::cin >> adminUser;
			std::cout << "Admin password: "; std::cin >> adminPass;
			if (admin.authenticate(adminUser, adminPass)) {
				admin.runMenu(auth);
			} else {
				std::cout << "Invalid admin credentials.\n";
			}
		} else break;
	}
	return 0;
}
