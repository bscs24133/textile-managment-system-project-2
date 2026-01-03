/*
 Fixed HTTP server with proper static file serving
 Build:
   g++ -std=c++17 server/Server.cpp -o server_bin -pthread
 Run:
   ./server_bin 8080
*/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <chrono>

// -------------------- small helpers --------------------
static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a==std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::map<std::string, std::string> parseJsonFlat(const std::string &s) {
    std::map<std::string,std::string> out;
    size_t i = 0, n = s.size();
    auto skip = [&](void){ while (i<n && isspace((unsigned char)s[i])) ++i; };
    skip();
    if (i>=n || s[i] != '{') return out;
    ++i;
    while (true) {
        skip();
        if (i>=n) break;
        if (s[i] == '}') { ++i; break; }
        if (s[i] != '"') break;
        ++i;
        std::string key;
        while (i<n && s[i] != '"') {
            if (s[i]=='\\' && i+1<n) { key.push_back(s[i+1]); i+=2; } else key.push_back(s[i++]);
        }
        if (i>=n || s[i]!='"') break;
        ++i;
        skip();
        if (i>=n || s[i] != ':') break;
        ++i;
        skip();
        std::string val;
        if (i<n && s[i] == '"') {
            ++i;
            while (i<n && s[i] != '"') {
                if (s[i]=='\\' && i+1<n) { val.push_back(s[i+1]); i+=2; } else val.push_back(s[i++]);
            }
            if (i>=n || s[i]!='"') break;
            ++i;
        } else {
            while (i<n && s[i] != ',' && s[i] != '}') { val.push_back(s[i++]); }
            val = trim(val);
        }
        out[key] = val;
        skip();
        if (i<n && s[i]==',') { ++i; continue; }
        if (i<n && s[i]=='}') { ++i; break; }
    }
    return out;
}

static std::string buildJsonObject(const std::map<std::string,std::string> &m) {
    std::ostringstream oss; oss << '{';
    bool first = true;
    for (auto &p : m) {
        if (!first) oss << ',';
        first = false;
        oss << '"' ;
        for (char c : p.first) { if (c=='"'||c=='\\') oss << '\\' << c; else oss << c; }
        oss << "\":\"";
        for (char c : p.second) { if (c=='"'||c=='\\') oss << '\\' << c; else oss << c; }
        oss << '"';
    }
    oss << '}';
    return oss.str();
}

static std::string buildJsonArray(const std::vector<std::map<std::string,std::string>> &arr) {
    std::ostringstream oss; oss << '[';
    bool first = true;
    for (auto &obj : arr) {
        if (!first) oss << ',';
        first = false;
        oss << buildJsonObject(obj);
    }
    oss << ']';
    return oss.str();
}

static ssize_t sendAll(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t r = send(fd, buf + sent, len - sent, 0);
        if (r <= 0) return r;
        sent += (size_t)r;
    }
    return (ssize_t)sent;
}

static bool fileExists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

static std::string readFileToString(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return "";
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

static std::string mimeFor(const std::string &path) {
    if (path.size() >= 5 && path.substr(path.size()-5)==".html") return "text/html; charset=utf-8";
    if (path.size() >= 4 && path.substr(path.size()-4)==".css") return "text/css";
    if (path.size() >= 3 && path.substr(path.size()-3)==".js") return "application/javascript";
    if (path.size() >= 4 && (path.substr(path.size()-4)==".png" || path.substr(path.size()-4)==".jpg")) {
        if (path.substr(path.size()-4)==".png") return "image/png";
        return "image/jpeg";
    }
    return "text/plain; charset=utf-8";
}

// -------------------- disk helpers --------------------
static std::string customersDataFile = "customers.data";
static std::string ordersDataFile = "orders.data";

static std::string hashPassword(const std::string &pw) {
    return std::to_string(std::hash<std::string>{}(pw));
}

static std::vector<std::map<std::string,std::string>> readAllCustomers() {
    std::vector<std::map<std::string,std::string>> out;
    std::ifstream ifs(customersDataFile, std::ios::binary);
    if (!ifs) return out;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::vector<std::string> toks;
        std::string cur;
        for (char c : line) { if (c == '|') { toks.push_back(cur); cur.clear(); } else cur.push_back(c); }
        toks.push_back(cur);
        if (toks.size() < 4) continue;
        std::map<std::string,std::string> m;
        m["id"] = toks[0];
        m["username"] = toks[1];
        m["passHash"] = toks[2];
        m["fullName"] = toks[3];
        out.push_back(m);
    }
    return out;
}

static std::vector<std::map<std::string,std::string>> readAllOrders() {
    std::vector<std::map<std::string,std::string>> out;
    std::ifstream ifs(ordersDataFile, std::ios::binary);
    if (!ifs) return out;
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::vector<std::string> toks;
        std::string cur;
        bool esc = false;
        for (size_t i=0;i<line.size();++i) {
            char c = line[i];
            if (!esc && c == '\\') { esc = true; continue; }
            if (!esc && c == '|') { toks.push_back(cur); cur.clear(); }
            else cur.push_back(c);
            esc = false;
        }
        toks.push_back(cur);
        if (toks.size() < 6) continue;
        std::map<std::string,std::string> m;
        m["orderID"] = toks[0];
        m["customerName"] = toks[1];
        m["productType"] = toks[2];
        m["quantity"] = toks[3];
        m["productionStage"] = toks[4];
        m["dateOfOrder"] = toks[5];
        out.push_back(m);
    }
    return out;
}

static int nextCustomerId() {
    std::ifstream ifs(customersDataFile);
    if (!ifs) return 1;
    int lines = 0;
    std::string l;
    while (std::getline(ifs,l)) if (!l.empty()) ++lines;
    return lines + 1;
}

static int nextOrderId() {
    std::ifstream ifs(ordersDataFile);
    if (!ifs) return 1;
    int lines = 0;
    std::string l;
    while (std::getline(ifs,l)) if (!l.empty()) ++lines;
    return lines + 1;
}

static bool appendOrderLine(const std::string &line) {
    std::ofstream ofs(ordersDataFile, std::ios::binary | std::ios::app);
    if (!ofs) return false;
    ofs << line << '\n';
    return true;
}

static bool appendCustomerLine(const std::string &line) {
    std::ofstream ofs(customersDataFile, std::ios::binary | std::ios::app);
    if (!ofs) return false;
    ofs << line << '\n';
    return true;
}

// -------------------- HTTP handling --------------------
class HttpHandler {
public:
    HttpHandler(int fd): fd_(fd) {}
    void handleConnection() {
        std::string req;
        if (!readRequest(req)) { close(fd_); return; }
        std::istringstream rs(req);
        std::string requestLine;
        std::getline(rs, requestLine);
        if (requestLine.back() == '\r') requestLine.pop_back();
        std::istringstream rl(requestLine);
        std::string method, path, proto;
        rl >> method >> path >> proto;
        
        std::map<std::string,std::string> headers;
        std::string line;
        size_t contentLength = 0;
        while (std::getline(rs,line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) break;
            auto pos = line.find(':');
            if (pos!=std::string::npos) {
                std::string key = trim(line.substr(0,pos));
                std::string val = trim(line.substr(pos+1));
                headers[key] = val;
                if (strcasecmp(key.c_str(),"Content-Length")==0) contentLength = std::stoul(val);
            }
        }
        
        std::string body;
        if (contentLength > 0) {
            body.resize(contentLength);
            size_t got = 0;
            while (got < contentLength) {
                ssize_t r = recv(fd_, &body[got], contentLength - got, 0);
                if (r <= 0) break;
                got += (size_t)r;
            }
        }
        
        std::cout << method << " " << path << std::endl;
        
        // Route
        if (method == "GET") {
            if (path == "/") {
                serveStatic("frontend/login.html");
            } else if (path.rfind("/api/",0) == 0) {
                handleApiGet(path);
            } else {
                // Remove leading slash and prepend frontend/
                std::string filePath = path;
                if (filePath[0] == '/') filePath = filePath.substr(1);
                filePath = "frontend/" + filePath;
                serveStatic(filePath);
            }
        } else if (method == "POST") {
            if (path.rfind("/api/",0) == 0) handleApiPost(path, body);
            else respond(404, "application/json", buildJsonObject({{"status","error"},{"error","not found"}}));
        } else {
            respond(405, "application/json", buildJsonObject({{"status","error"},{"error","method not allowed"}}));
        }
        close(fd_);
    }
private:
    int fd_;
    
    bool readRequest(std::string &out) {
        out.clear();
        char buf[1024];
        std::string acc;
        while (true) {
            ssize_t r = recv(fd_, buf, sizeof(buf), 0);
            if (r <= 0) { return false; }
            acc.append(buf, buf + r);
            if (acc.find("\r\n\r\n") != std::string::npos) break;
            if (acc.size() > 64*1024) break;
        }
        out = acc;
        return true;
    }
    
    void serveStatic(const std::string &filepath) {
        if (!fileExists(filepath)) {
            std::cout << "File not found: " << filepath << std::endl;
            respond(404, "text/html", "<html><body><h1>404 Not Found</h1><p>File: " + filepath + "</p></body></html>");
            return;
        }
        std::string content = readFileToString(filepath);
        if (content.empty()) {
            respond(404, "text/html", "<html><body><h1>404 Not Found</h1></body></html>");
            return;
        }
        std::string mime = mimeFor(filepath);
        respond(200, mime, content);
    }
    
    void handleApiGet(const std::string &path) {
        if (path == "/api/customers") {
            auto customers = readAllCustomers();
            std::string body = buildJsonArray(customers);
            respond(200, "application/json", body);
            return;
        } else if (path == "/api/orders") {
            auto orders = readAllOrders();
            std::string body = buildJsonArray(orders);
            respond(200, "application/json", body);
            return;
        }
        respond(404, "application/json", buildJsonObject({{"status","error"},{"error","not found"}}));
    }
    
    void handleApiPost(const std::string &path, const std::string &body) {
        if (path == "/api/login") {
            auto req = parseJsonFlat(body);
            std::string username = req.count("username") ? req.at("username") : "";
            std::string password = req.count("password") ? req.at("password") : "";
            if (username.empty() || password.empty()) {
                respond(400,"application/json", buildJsonObject({{"status","error"},{"error","missing fields"}}));
                return;
            }
            auto customers = readAllCustomers();
            for (auto &c : customers) {
                if (c.at("username") == username) {
                    if (c.at("passHash") == hashPassword(password)) {
                        respond(200,"application/json", buildJsonObject({{"status","ok"},{"id",c.at("id")},{"fullName",c.at("fullName")}}));
                        return;
                    } else {
                        respond(403,"application/json", buildJsonObject({{"status","error"},{"error","invalid credentials"}}));
                        return;
                    }
                }
            }
            respond(404,"application/json", buildJsonObject({{"status","error"},{"error","user not found"}}));
            return;
        } else if (path == "/api/signup") {
            auto req = parseJsonFlat(body);
            std::string username = req.count("username") ? req.at("username") : "";
            std::string password = req.count("password") ? req.at("password") : "";
            std::string fullName = req.count("fullName") ? req.at("fullName") : "";
            if (username.empty() || password.empty() || fullName.empty()) {
                respond(400,"application/json", buildJsonObject({{"status","error"},{"error","missing fields"}}));
                return;
            }
            auto customers = readAllCustomers();
            for (auto &c : customers) if (c.at("username") == username) {
                respond(409,"application/json", buildJsonObject({{"status","error"},{"error","user exists"}}));
                return;
            }
            int id = nextCustomerId();
            std::ostringstream oss;
            oss << id << '|' << username << '|' << hashPassword(password) << '|' << fullName;
            if (!appendCustomerLine(oss.str())) {
                respond(500,"application/json", buildJsonObject({{"status","error"},{"error","failed to save"}}));
                return;
            }
            respond(200,"application/json", buildJsonObject({{"status","ok"},{"id",std::to_string(id)}}));
            return;
        } else if (path == "/api/orders") {
            auto req = parseJsonFlat(body);
            std::string customerName = req.count("customerName") ? req.at("customerName") : "";
            std::string productType = req.count("productType") ? req.at("productType") : "";
            std::string quantity = req.count("quantity") ? req.at("quantity") : "";
            std::string dateOfOrder = req.count("dateOfOrder") ? req.at("dateOfOrder") : "";
            if (customerName.empty() || productType.empty() || quantity.empty() || dateOfOrder.empty()) {
                respond(400,"application/json", buildJsonObject({{"status","error"},{"error","missing fields"}}));
                return;
            }
            int oid = nextOrderId();
            std::ostringstream oss;
            auto esc = [](const std::string &s)->std::string {
                std::string r;
                for (char c : s) { if (c=='|'||c=='\\') { r.push_back('\\'); r.push_back(c); } else r.push_back(c); }
                return r;
            };
            oss << oid << '|' << esc(customerName) << '|' << esc(productType) << '|' << quantity << '|' << "Received" << '|' << dateOfOrder;
            if (!appendOrderLine(oss.str())) {
                respond(500,"application/json", buildJsonObject({{"status","error"},{"error","failed to save order"}}));
                return;
            }
            respond(200,"application/json", buildJsonObject({{"status","ok"},{"orderID",std::to_string(oid)}}));
            return;
        }
        respond(404,"application/json", buildJsonObject({{"status","error"},{"error","not found"}}));
    }
    
    void respond(int status, const std::string &contentType, const std::string &body) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << " OK\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Connection: close\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "\r\n";
        std::string hdr = oss.str();
        sendAll(fd_, hdr.c_str(), hdr.size());
        if (!body.empty()) sendAll(fd_, body.c_str(), body.size());
    }
};

// -------------------- server --------------------
int main(int argc, char **argv) {
    int port = 8080;
    if (argc >= 2) port = std::stoi(argv[1]);
    
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) { perror("socket"); return 1; }
    
    int on = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    struct sockaddr_in addr;
    std::memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    int tries = 0;
    while (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        ++tries;
        if (tries >= 5) { close(listenFd); return 1; }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    if (listen(listenFd, 20) < 0) { perror("listen"); close(listenFd); return 1; }
    
    std::cout << "========================================\n";
    std::cout << "HTTP Server started successfully!\n";
    std::cout << "Listening on port: " << port << "\n";
    std::cout << "========================================\n";
    std::cout << "Access the application at:\n";
    std::cout << "  http://localhost:" << port << "/login.html\n";
    std::cout << "========================================\n";
    std::cout << "Server logs:\n";
    
    while (true) {
        struct sockaddr_in cli;
        socklen_t len = sizeof(cli);
        int client = accept(listenFd, (struct sockaddr*)&cli, &len);
        if (client < 0) continue;
        std::thread([client]() {
            HttpHandler h(client);
            h.handleConnection();
        }).detach();
    }
    
    close(listenFd);
    return 0;
}