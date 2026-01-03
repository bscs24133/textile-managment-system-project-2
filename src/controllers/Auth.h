#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "../models/Customer.h"

class AuthController {
public:
	AuthController();
	~AuthController();

	// returns nullptr on failure (or existing customer on success)
	Customer* signup(const std::string &username, const std::string &password, const std::string &fullName);

	// returns customer pointer on success, nullptr on failure
	Customer* login(const std::string &username, const std::string &password);

	// load index from disk (called in ctor)
	void loadIndex();

	// NEW: return a Customer* by username without password check (caller owns returned pointer)
	// Returns nullptr if username not found or file cannot be read.
	Customer* getCustomerByUsername(const std::string &username) const;

	// NEW: return all customers loaded from customers.data (values copied)
	std::vector<Customer> getAllCustomers() const;

private:
	std::unordered_map<std::string, long long> indexMap; // username -> offset
	const std::string DATA_FILE = "customers.data";
	const std::string INDEX_FILE = "customers.index";

	int nextId();
	std::string hashPassword(const std::string &password);
};
