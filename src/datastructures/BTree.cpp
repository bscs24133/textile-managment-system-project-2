#include "BTree.h"
#include <algorithm>
#include <iostream>

BTreeNode::BTreeNode(int _t, bool _leaf) : t(_t), leaf(_leaf) {
	keys.reserve(2 * t - 1);
	values.reserve(2 * t - 1);
	C.reserve(2 * t);
}

BTreeNode::~BTreeNode() {
	for (auto c : C) delete c;
}

Order* BTreeNode::search(int k) {
	int i = 0;
	while (i < (int)keys.size() && k > keys[i]) ++i;
	if (i < (int)keys.size() && keys[i] == k) return values[i];
	if (leaf) return nullptr;
	return C[i]->search(k);
}

void BTreeNode::insertNonFull(int k, Order* orderPtr) {
	int i = (int)keys.size() - 1;
	if (leaf) {
		keys.push_back(0);
		values.push_back(nullptr);
		while (i >= 0 && keys[i] > k) {
			keys[i + 1] = keys[i];
			values[i + 1] = values[i];
			--i;
		}
		keys[i + 1] = k;
		values[i + 1] = orderPtr;
	} else {
		while (i >= 0 && keys[i] > k) --i;
		++i;
		if ((int)C[i]->keys.size() == 2 * t - 1) {
			splitChild(i, C[i]);
			if (keys[i] < k) ++i;
		}
		C[i]->insertNonFull(k, orderPtr);
	}
}

void BTreeNode::splitChild(int i, BTreeNode* y) {
	BTreeNode* z = new BTreeNode(y->t, y->leaf);
	int t_ = y->t;
	// transfer last t-1 keys/values to z
	for (int j = 0; j < t_ - 1; ++j) {
		z->keys.push_back(y->keys[j + t_]);
		z->values.push_back(y->values[j + t_]);
	}
	if (!y->leaf) {
		for (int j = 0; j < t_; ++j) {
			z->C.push_back(y->C[j + t_]);
		}
	}
	// reduce y
	y->keys.resize(t_ - 1);
	y->values.resize(t_ - 1);
	if (!y->leaf) y->C.resize(t_);
	// insert new child into this node
	C.insert(C.begin() + i + 1, z);
	keys.insert(keys.begin() + i, y->keys[t_ - 1]);
	values.insert(values.begin() + i, y->values[t_ - 1]);
	// remove the median from y (already done via resize, but ensure)
	// y->keys and y->values were resized
}

BTree::BTree(int _t) : root(nullptr), t(_t) {}

BTree::~BTree() { delete root; }

void BTree::insert(int k, Order* orderPtr) {
	if (!root) {
		root = new BTreeNode(t, true);
		root->keys.push_back(k);
		root->values.push_back(orderPtr);
		return;
	}
	// if key exists, update pointer
	if (root->search(k) != nullptr) {
		// find and replace
		BTreeNode* cur = root;
		while (cur) {
			int i = 0;
			while (i < (int)cur->keys.size() && k > cur->keys[i]) ++i;
			if (i < (int)cur->keys.size() && cur->keys[i] == k) {
				cur->values[i] = orderPtr;
				return;
			}
			if (cur->leaf) break;
			cur = cur->C[i];
		}
	}
	if ((int)root->keys.size() == 2 * t - 1) {
		BTreeNode* s = new BTreeNode(t, false);
		s->C.push_back(root);
		s->splitChild(0, root);
		int i = 0;
		if (s->keys[0] < k) ++i;
		s->C[i]->insertNonFull(k, orderPtr);
		root = s;
	} else {
		root->insertNonFull(k, orderPtr);
	}
}

Order* BTree::search(int k) {
	if (!root) return nullptr;
	return root->search(k);
}
