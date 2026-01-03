#pragma once
#include <vector>
#include <string>
#include <functional>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <ctime>
#include <iostream>

template<typename T>
class BTreeNode {
public:
	int t;
	bool leaf;
	std::vector<int> keys;
	std::vector<T> values;
	std::vector<BTreeNode<T>*> C;

	BTreeNode(int _t, bool _leaf) : t(_t), leaf(_leaf) {
		keys.reserve(2 * t - 1);
		values.reserve(2 * t - 1);
		C.reserve(2 * t);
	}
	~BTreeNode() {
		for (auto c : C) delete c;
	}
	T search(int k) {
		int i = 0;
		while (i < (int)keys.size() && k > keys[i]) ++i;
		if (i < (int)keys.size() && keys[i] == k) return values[i];
		if (leaf) return nullptr;
		return C[i]->search(k);
	}
	void insertNonFull(int k, T val) {
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
			values[i + 1] = val;
		} else {
			while (i >= 0 && keys[i] > k) --i;
			++i;
			if ((int)C[i]->keys.size() == 2 * t - 1) {
				splitChild(i, C[i]);
				if (keys[i] < k) ++i;
			}
			C[i]->insertNonFull(k, val);
		}
	}
	void splitChild(int i, BTreeNode<T>* y) {
		BTreeNode<T>* z = new BTreeNode<T>(y->t, y->leaf);
		int t_ = y->t;
		for (int j = 0; j < t_ - 1; ++j) {
			z->keys.push_back(y->keys[j + t_]);
			z->values.push_back(y->values[j + t_]);
		}
		if (!y->leaf) {
			for (int j = 0; j < t_; ++j) z->C.push_back(y->C[j + t_]);
		}
		y->keys.resize(t_ - 1);
		y->values.resize(t_ - 1);
		if (!y->leaf) y->C.resize(t_);
		C.insert(C.begin() + i + 1, z);
		keys.insert(keys.begin() + i, y->keys[t_ - 1]);
		values.insert(values.begin() + i, y->values[t_ - 1]);
	}
};

// header-only BTree template
template<typename T>
class BTree {
public:
	// serializer: T -> string line; deserializer: string -> T
	using Serializer = std::function<std::string(T)>;
	using Deserializer = std::function<T(const std::string&)>;

	BTree(int _t,
	      Serializer serializer = nullptr,
	      Deserializer deserializer = nullptr,
	      const std::string &dataFile = "orders.data",
	      const std::string &indexFile = "orders.index",
	      const std::string &metaFile = "orders.meta")
		: root(nullptr), t(_t),
		  serializeFn(serializer), deserializeFn(deserializer),
		  DATA_FILE(dataFile), INDEX_FILE(indexFile), META_FILE(metaFile)
	{
		loadFromDiskInto();
	}

	~BTree() { delete root; }

	// insert key -> value (value is a pointer T, e.g. Order*)
	void insert(int k, T val) {
		if (!g_loading && serializeFn) {
			persistToDisk(k, val);
			// update meta with actual t
			std::ofstream meta(META_FILE, std::ios::trunc);
			if (meta) { meta << t << ' ' << std::time(nullptr) << '\n'; meta.close(); }
		}

		if (!root) {
			root = new BTreeNode<T>(t, true);
			root->keys.push_back(k);
			root->values.push_back(val);
			return;
		}
		// if key exists, replace pointer
		if (root->search(k) != nullptr) {
			BTreeNode<T>* cur = root;
			while (cur) {
				int i = 0;
				while (i < (int)cur->keys.size() && k > cur->keys[i]) ++i;
				if (i < (int)cur->keys.size() && cur->keys[i] == k) {
					cur->values[i] = val;
					return;
				}
				if (cur->leaf) break;
				cur = cur->C[i];
			}
		}
		if ((int)root->keys.size() == 2 * t - 1) {
			BTreeNode<T>* s = new BTreeNode<T>(t, false);
			s->C.push_back(root);
			s->splitChild(0, root);
			int i = 0;
			if (s->keys[0] < k) ++i;
			s->C[i]->insertNonFull(k, val);
			root = s;
		} else {
			root->insertNonFull(k, val);
		}
	}

	// lookup
	T search(int k) {
		if (!root) return nullptr;
		return root->search(k);
	}

	// persisted items loaded from disk during construction
	void registerPersistedItem(T item) { if (item) persistedItems.push_back(item); }
	std::vector<T> getPersistedItems() const { return persistedItems; }

private:
	BTreeNode<T>* root;
	int t;
	std::vector<T> persistedItems;
	Serializer serializeFn;
	Deserializer deserializeFn;
	const std::string DATA_FILE;
	const std::string INDEX_FILE;
	const std::string META_FILE;

	static bool g_loading;

	// helper escape/unescape (kept simple)
	static std::string escapeField(const std::string &s) {
		std::string out;
		for (char c : s) {
			if (c == '|') { out += '\\'; out += '|'; }
			else if (c == '\\') { out += '\\'; out += '\\'; }
			else out += c;
		}
		return out;
	}
	static std::string unescapeField(const std::string &s) {
		std::string out;
		for (size_t i = 0; i < s.size(); ++i) {
			if (s[i] == '\\' && i + 1 < s.size()) {
				out += s[i+1];
				++i;
			} else out += s[i];
		}
		return out;
	}

	// persist using serializer
	void persistToDisk(int key, T val) {
		if (!serializeFn) return;
		std::ofstream data(DATA_FILE, std::ios::binary | std::ios::app);
		if (!data) { std::cerr << "Failed to open " << DATA_FILE << '\n'; return; }
		std::streampos offset = data.tellp();
		std::string line = serializeFn(val);
		data << line << '\n';
		data.close();
		std::ofstream idx(INDEX_FILE, std::ios::app);
		if (!idx) { std::cerr << "Failed to open " << INDEX_FILE << '\n'; return; }
		idx << key << ' ' << offset << '\n';
		idx.close();
	}

	// load helpers
	void loadFromDataFileInto() {
		if (!deserializeFn) return;
		std::ifstream data(DATA_FILE, std::ios::binary);
		if (!data) return;
		g_loading = true;
		std::string rec;
		while (std::getline(data, rec)) {
			if (rec.empty()) continue;
			T item = deserializeFn(rec);
			if (!item) continue;
			insert(itemKey(item), item); // insert into tree (will not re-persist because g_loading true)
			registerPersistedItem(item);
		}
		g_loading = false;
		data.close();
	}
	void loadFromDiskInto() {
		if (!deserializeFn) { return; }
		std::ifstream idx(INDEX_FILE);
		if (!idx) { loadFromDataFileInto(); return; }
		std::unordered_map<int, std::streampos> lastPos;
		std::string line;
		while (std::getline(idx, line)) {
			if (line.empty()) continue;
			std::istringstream iss(line);
			int id; long long pos;
			if (!(iss >> id >> pos)) continue;
			lastPos[id] = static_cast<std::streampos>(pos);
		}
		idx.close();
		if (lastPos.empty()) { loadFromDataFileInto(); return; }
		std::ifstream data(DATA_FILE, std::ios::binary);
		if (!data) return;
		g_loading = true;
		for (const auto &p : lastPos) {
			int id = p.first; std::streampos pos = p.second;
			data.clear();
			data.seekg(pos);
			std::string rec;
			if (!std::getline(data, rec)) continue;
			T item = deserializeFn(rec);
			if (!item) continue;
			insert(id, item);
			registerPersistedItem(item);
		}
		g_loading = false;
		data.close();
	}

	// fallback key extractor: user must specialize or provide itemKey via ADL by defining free function:
	// int itemKey(T item);
	// For convenience, if T has `->orderID` or `->id` fields, we detect them (basic).
	// As fallback return 0.
	static int itemKey(T item) {
		(void)item; // silence unused-parameter warning for types where we don't extract a key
		return 0;
	}
};

// define static member at global scope; use different template parameter name to avoid shadowing
template<typename U>
bool BTree<U>::g_loading = false;
