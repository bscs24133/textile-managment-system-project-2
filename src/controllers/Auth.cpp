#include "Auth.h"
#include <fstream>
#include <sstream>
#include <functional>
#include <sys/stat.h>
#include <vector>

static bool fileExists(const std::string &path) {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

AuthController::AuthController() { loadIndex(); }
AuthController::~AuthController() {}

void AuthController::loadIndex() {
	indexMap.clear();
	std::ifstream idx(INDEX_FILE);
	if (!idx) return;
	std::string line;
	while (std::getline(idx, line)) {
		if (line.empty()) continue;
		std::istringstream iss(line);
		std::string username;
		long long pos;
		if (!(iss >> username >> pos)) continue;
		indexMap[username] = pos;
	}
}

int AuthController::nextId() {
	if (!fileExists(DATA_FILE)) return 1;
	std::ifstream data(DATA_FILE);
	int count = 0;
	std::string line;
	while (std::getline(data, line)) if (!line.empty()) ++count;
	return count + 1;
}

std::string AuthController::hashPassword(const std::string &password) {
	std::size_t h = std::hash<std::string>{}(password);
	std::ostringstream oss; oss << h; return oss.str();
}

Customer* AuthController::signup(const std::string &username, const std::string &password, const std::string &fullName) {
	if (username.empty() || password.empty()) return nullptr;
	if (indexMap.find(username) != indexMap.end()) return nullptr;
	int id = nextId();
	std::string hashed = hashPassword(password);
	Customer c(id, username, hashed, fullName);
	std::string serialized = c.serialize();
	std::ofstream data(DATA_FILE, std::ios::binary | std::ios::app);
	if (!data) return nullptr;
	std::streampos offset = data.tellp();
	data << serialized << '\n';
	data.close();
	std::ofstream idx(INDEX_FILE, std::ios::app);
	if (!idx) return nullptr;
	idx << username << ' ' << offset << '\n';
	idx.close();
	indexMap[username] = (long long)offset;
	return new Customer(c);
}

Customer* AuthController::login(const std::string &username, const std::string &password) {
	auto it = indexMap.find(username);
	if (it == indexMap.end()) return nullptr;
	long long offset = it->second;
	std::ifstream data(DATA_FILE, std::ios::binary);
	if (!data) return nullptr;
	data.seekg(offset);
	std::string line;
	if (!std::getline(data, line)) return nullptr;
	Customer* c = Customer::deserialize(line);
	if (!c) return nullptr;
	if (c->passHash != hashPassword(password)) { delete c; return nullptr; }
	return c;
}

Customer* AuthController::getCustomerByUsername(const std::string &username) const {
	auto it = indexMap.find(username);
	if (it == indexMap.end()) return nullptr;
	long long offset = it->second;
	std::ifstream data(DATA_FILE, std::ios::binary);
	if (!data) return nullptr;
	data.seekg(offset);
	std::string line;
	if (!std::getline(data, line)) return nullptr;
	Customer* c = Customer::deserialize(line);
	return c; // caller must delete
}

std::vector<Customer> AuthController::getAllCustomers() const {
	std::vector<Customer> out;
	std::ifstream data(DATA_FILE, std::ios::binary);
	if (!data) return out;
	std::string line;
	while (std::getline(data, line)) {
		if (line.empty()) continue;
		Customer* c = Customer::deserialize(line);
		if (!c) continue;
		out.push_back(*c);
		delete c;
	}
	return out;
}