#pragma once
#include "../models/Order.h"
#include <vector>

class BTreeNode {
public:
	int t; // minimum degree
	bool leaf;
	std::vector<int> keys;
	std::vector<Order*> values;
	std::vector<BTreeNode*> C;

	BTreeNode(int _t, bool _leaf);
	~BTreeNode();

	Order* search(int k);
	void insertNonFull(int k, Order* orderPtr);
	void splitChild(int i, BTreeNode* y);
};

class BTree {
public:
	BTree(int _t = 2);
	~BTree();
	void insert(int k, Order* orderPtr);
	Order* search(int k);

private:
	BTreeNode* root;
	int t;
};
