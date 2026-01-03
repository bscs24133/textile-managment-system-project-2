#pragma once
#include <string>
#include <sstream>
#include <vector>

class Customer {
public:
	int id;
	std::string username;
	std::string passHash; // simple hash placeholder (replace with proper hashing lib later)
	std::string fullName;

	Customer() : id(0) {}
	Customer(int _id, const std::string &_username, const std::string &_passHash, const std::string &_fullName)
		: id(_id), username(_username), passHash(_passHash), fullName(_fullName) {}

	std::string serialize() const {
		// id|username|passHash|fullName
		std::ostringstream oss;
		oss << id << '|' << username << '|' << passHash << '|' << fullName;
		return oss.str();
	}

	static Customer* deserialize(const std::string &line) {
		// simple split respecting no escapes (fields are plain here)
		std::vector<std::string> toks;
		std::string cur;
		for (char c : line) {
			if (c == '|') { 
				toks.push_back(cur);
				 cur.clear(); 
				}
			else cur.push_back(c);
		}
		toks.push_back(cur);
		if (toks.size() < 4) return nullptr;
		int id = std::stoi(toks[0]);
		return new Customer(id, toks[1], toks[2], toks[3]);
	}
};
