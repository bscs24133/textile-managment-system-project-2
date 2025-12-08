#include "core/SystemController.h"
#include <iostream>

int main() {
	SystemController controller;
	int choice = 0;
	while (true) {
		std::cout << "-------------------------------------------\n"
		          << " TEXTILE ORDER TRACKING SYSTEM (BACKEND)\n"
		          << "-------------------------------------------\n"
		          << "1. Add New Order\n"
		          << "2. Search Order by ID (B-Tree)\n"
		          << "3. Search Orders by Customer Name (Hash Table)\n"
		          << "4. Advance Production Stage (Queue)\n"
		          << "5. View All Orders\n"
		          << "6. View Production Queue\n"
		          << "7. Exit\n"
		          << "Enter choice: ";
		if (!(std::cin >> choice)) {
			std::cin.clear();
			std::cin.ignore(10000, '\n');
			continue;
		}
		switch (choice) {
			case 1: controller.addOrder(); break;
			case 2: controller.searchByID(); break;
			case 3: controller.searchByCustomer(); break;
			case 4: controller.advanceStage(); break;
			case 5: controller.viewAllOrders(); break;
			case 6: controller.viewProductionQueue(); break;
			case 7: std::cout << "Exiting...\n"; return 0;
			default: std::cout << "Invalid choice.\n";
		}
		std::cout << "\n";
	}
	return 0;
}
