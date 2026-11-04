
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <deque>
#include <fstream>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
using socket_t = int;
#endif

using namespace std;

const size_t MAXIMUM_QUEUE_CAPACITY = 20;
const size_t MAX_HEADER_SIZE = 32768;
const size_t MAX_BODY_SIZE = 1024 * 1024;

string htmlEscape(const string& text) {
    string out;
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&#39;"; break;
        default: out += c;
        }
    }
    return out;
}

optional<int> toInt(const string& text) {
    try {
        size_t used = 0;
        int value = stoi(text, &used);
        if (used != text.size()) return nullopt;
        return value;
    }
    catch (...) {
        return nullopt;
    }
}

struct Vehicle {
    string registrationNumber;
    string vehicleType;
};

struct Road {
    int destinationIntersectionId;
    string roadName;
    bool isBlocked = false;
    deque<Vehicle> waitingQueue;

    bool enqueueVehicle(Vehicle v) {
        if (waitingQueue.size() >= MAXIMUM_QUEUE_CAPACITY)
            return false;

        waitingQueue.push_back(std::move(v));
        return true;
    }

    string queueHtml() const {
        if (waitingQueue.empty())
            return "<em>Queue is empty</em>";

        ostringstream html;
        html << "<ul>";

        for (const Vehicle& v : waitingQueue) {
            html << "<li>"
                 << htmlEscape(v.registrationNumber)
                 << " (" << htmlEscape(v.vehicleType) << ")</li>";
        }

        html << "</ul>";
        return html.str();
    }
};

struct Intersection {
    int intersectionId;
    string intersectionName;
    list<Road> roads;

    Intersection(int id, string name)
        : intersectionId(id), intersectionName(std::move(name)) {}

    Road* findRoad(int destId) {
        auto it = find_if(roads.begin(), roads.end(),
            [destId](const Road& r) {
                return r.destinationIntersectionId == destId;
            });

        return it == roads.end() ? nullptr : &*it;
    }

    Road* findRoadByName(const string& name) {
        auto it = find_if(roads.begin(), roads.end(),
            [&name](const Road& r) {
                return r.roadName == name;
            });

        return it == roads.end() ? nullptr : &*it;
    }

    bool hasRoadNamed(const string& name) const {
        return any_of(roads.begin(), roads.end(),
            [&name](const Road& r) {
                return r.roadName == name;
            });
    }

    bool hasRoadTo(int destId) const {
        return any_of(roads.begin(), roads.end(),
            [destId](const Road& r) {
                return r.destinationIntersectionId == destId;
            });
    }

    bool setRoadBlocked(int destId, bool blocked) {
        Road* r = findRoad(destId);
        if (!r) return false;

        r->isBlocked = blocked;
        return true;
    }

    size_t totalWaiting() const {
        size_t n = 0;
        for (const Road& r : roads)
            n += r.waitingQueue.size();

        return n;
    }

    string roadsHtml() const {
        if (roads.empty())
            return "<em>No outgoing roads</em>";

        ostringstream html;
        html << "<ul>";

        for (const Road& r : roads) {
            html << "<li>To ID: "
                 << r.destinationIntersectionId
                 << " via " << htmlEscape(r.roadName)
                 << (r.isBlocked
                    ? " <strong style='color:red;'>[BLOCKED]</strong>"
                    : " <strong style='color:green;'>[OPEN]</strong>")
                 << "<br><small>Waiting ("
                 << r.waitingQueue.size()
                 << "):</small>"
                 << r.queueHtml()
                 << "</li>";
        }

        html << "</ul>";
        return html.str();
    }
};

class CityTrafficNetwork {
private:
    map<int, Intersection> intersections;

    Intersection* find(int id) {
        auto it = intersections.find(id);
        return it == intersections.end() ? nullptr : &it->second;
    }

public:
    string addIntersection(int newId, const string& newName) {
        if (!intersections.emplace(
            newId, Intersection(newId, newName)).second)
            return "Intersection ID " + to_string(newId) +
                   " already exists.";

        return "Intersection '" + htmlEscape(newName) +
               "' (ID: " + to_string(newId) +
               ") added successfully.";
    }

    string connectIntersections(
        int firstId, int secondId, const string& roadName) {

        Intersection* a = find(firstId);
        Intersection* b = find(secondId);

        if (!a || !b)
            return "Invalid intersection IDs provided.";

        if (firstId == secondId)
            return "Cannot connect an intersection to itself.";

        if (a->hasRoadTo(secondId))
            return "A road already exists between these intersections.";

        if (a->hasRoadNamed(roadName) ||
            b->hasRoadNamed(roadName))
            return "Road name '" + htmlEscape(roadName) +
                   "' is already used at one of these intersections.";

        a->roads.push_front(Road{secondId, roadName, false, {}});
        b->roads.push_front(Road{firstId, roadName, false, {}});

        return "Road '" + htmlEscape(roadName) +
               "' connected between Intersection " +
               to_string(firstId) + " and " +
               to_string(secondId) + ".";
    }

    string addVehicleToRoad(
        int id, const string& roadName,
        const string& vehicleNumber, const string& vehicleType) {

        Intersection* node = find(id);

        if (!node)
            return "Intersection ID " + to_string(id) + " not found.";

        Road* road = node->findRoadByName(roadName);

        if (!road)
            return "No road named '" + htmlEscape(roadName) +
                   "' at intersection " + to_string(id) + ".";

        if (road->isBlocked)
            return "This road is blocked. Vehicles cannot enter.";

        if (road->enqueueVehicle({vehicleNumber, vehicleType})) {
            return "Vehicle " + htmlEscape(vehicleNumber) +
                   " (" + htmlEscape(vehicleType) +
                   ") queued at " + htmlEscape(node->intersectionName) +
                   " for road '" + htmlEscape(road->roadName) + "'.";
        }

        return "Queue is full on road '" +
               htmlEscape(road->roadName) + "'.";
    }

    string allowVehiclesToPass(
        int id, const string& roadName, int count) {

        Intersection* node = find(id);

        if (!node)
            return "Intersection ID " + to_string(id) + " not found.";

        Road* road = node->findRoadByName(roadName);

        if (!road)
            return "No road named '" + htmlEscape(roadName) +
                   "' at intersection " + to_string(id) + ".";

        if (count <= 0)
            return "Number of vehicles must be positive.";

        if (road->isBlocked)
            return "Road '" + htmlEscape(roadName) +
                   "' is BLOCKED. No vehicles can pass.";

        size_t passed = min(
            static_cast<size_t>(count),
            road->waitingQueue.size()
        );

        road->waitingQueue.erase(
            road->waitingQueue.begin(),
            road->waitingQueue.begin() + passed
        );

        return "SIGNAL GREEN at " +
               htmlEscape(node->intersectionName) +
               " for road '" + htmlEscape(road->roadName) +
               "'. " + to_string(passed) +
               " vehicle(s) passed. Remaining: " +
               to_string(road->waitingQueue.size()) + ".";
    }

    string changeRoadBlockStatus(
        int firstId, int secondId, bool blocked) {

        Intersection* a = find(firstId);
        Intersection* b = find(secondId);

        if (!a || !b)
            return "Invalid intersection IDs.";

        Road* forward = a->findRoad(secondId);
        Road* reverse = b->findRoad(firstId);

        if (!forward || !reverse)
            return "Road does not exist between these two intersections.";

        forward->isBlocked = blocked;
        reverse->isBlocked = blocked;

        return "Road between " + to_string(firstId) +
               " and " + to_string(secondId) +
               (blocked ? " is now BLOCKED." : " is now REOPENED.");
    }

    string getNetworkHtml() const {
        if (intersections.empty())
            return "<p>No intersections registered in the network.</p>";

        ostringstream html;

        for (const auto& [id, node] : intersections) {
            html << "<div style='background:#f4f4f4;padding:12px;"
                 << "margin-bottom:12px;border-radius:6px;"
                 << "border:1px solid #ccc;'>"
                 << "<h3>Intersection [" << id << "] "
                 << htmlEscape(node.intersectionName) << "</h3>"
                 << "<p><strong>Total waiting vehicles: "
                 << node.totalWaiting() << "</strong></p>"
                 << "<p><strong>Roads &amp; their queues:</strong></p>"
                 << node.roadsHtml()
                 << "</div>";
        }

        return html.str();
    }
};

CityTrafficNetwork simulator;

string urlDecode(const string& text) {
    string result;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') {
            result += ' ';
        }
        else if (text[i] == '%' && i + 2 < text.size()) {
            try {
                result += static_cast<char>(
                    stoi(text.substr(i + 1, 2), nullptr, 16)
                );
                i += 2;
            }
            catch (...) {
                result += text[i];
            }
        }
        else {
            result += text[i];
        }
    }

    return result;
}

map<string, string> parseFormBody(const string& body) {
    map<string, string> parsed;
    istringstream stream(body);
    string pair;

    while (getline(stream, pair, '&')) {
        size_t eq = pair.find('=');

        if (eq != string::npos) {
            parsed[urlDecode(pair.substr(0, eq))] =
                urlDecode(pair.substr(eq + 1));
        }
    }

    return parsed;
}

void replaceFirst(
    string& text, const string& token, const string& replacement) {

    size_t pos = text.find(token);

    if (pos != string::npos)
        text.replace(pos, token.size(), replacement);
}


string renderPage(const string& message) {
    ifstream file("templates/index.html");

    if (!file.is_open()) {
        cerr << "ERROR: templates/index.html not found" << endl;
        return "<h1>Server configuration error: HTML template missing</h1>";
    }

    string content{
        istreambuf_iterator<char>(file),
        istreambuf_iterator<char>{}
    };

    string messageHtml;

    if (!message.empty()) {
        messageHtml =
            "<div style='padding:10px;background:#e0f2fe;"
            "border:1px solid #0284c7;color:#0369a1;"
            "border-radius:4px;margin-bottom:15px;font-weight:bold;'>"
            + message + "</div>";
    }

    replaceFirst(content, "{{MESSAGE}}", messageHtml);
    replaceFirst(content, "{{NETWORK}}", simulator.getNetworkHtml());

    return content;
}

string processAction(const map<string, string>& form) {
    auto get = [&](const string& key) -> string {
        auto it = form.find(key);
        return it == form.end() ? "" : it->second;
    };

    const string action = get("action");

    if (action == "add_intersection") {
        auto id = toInt(get("id"));

        if (id && !get("name").empty())
            return simulator.addIntersection(*id, get("name"));
    }
    else if (action == "connect_road") {
        auto src = toInt(get("src"));
        auto dest = toInt(get("dest"));

        if (src && dest && !get("name").empty())
            return simulator.connectIntersections(
                *src, *dest, get("name"));
    }
    else if (action == "add_vehicle") {
        auto id = toInt(get("id"));

        if (id && !get("road").empty() &&
            !get("reg").empty() && !get("type").empty()) {

            return simulator.addVehicleToRoad(
                *id, get("road"), get("reg"), get("type"));
        }
    }
    else if (action == "green_signal") {
        auto id = toInt(get("id"));
        auto count = toInt(get("count"));

        if (id && !get("road").empty() && count)
            return simulator.allowVehiclesToPass(
                *id, get("road"), *count);
    }
    else if (action == "block_road" ||
             action == "reopen_road") {

        auto src = toInt(get("src"));
        auto dest = toInt(get("dest"));

        if (src && dest)
            return simulator.changeRoadBlockStatus(
                *src, *dest, action == "block_road");
    }

    return "Invalid or incomplete form submission.";
}

void closeSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void setSocketTimeout(socket_t s) {
#ifdef _WIN32
    DWORD timeout = 15000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;

    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
        &timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO,
        &timeout, sizeof(timeout));
#endif
}

bool sendAll(socket_t s, const string& data) {
    size_t sent = 0;

    while (sent < data.size()) {
        size_t remaining = data.size() - sent;
        int chunk = static_cast<int>(
            min(remaining, static_cast<size_t>(65536))
        );

#ifdef MSG_NOSIGNAL
        int n = send(s, data.data() + sent, chunk, MSG_NOSIGNAL);
#else
        int n = send(s, data.data() + sent, chunk, 0);
#endif

        if (n <= 0)
            return false;

        sent += static_cast<size_t>(n);
    }

    return true;
}

void sendResponse(
    socket_t client, int status,
    const string& statusText, const string& body) {

    string response =
        "HTTP/1.1 " + to_string(status) + " " + statusText + "\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n" + body;

    sendAll(client, response);
}

string lowerString(string s) {
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });

    return s;
}

// Read a complete HTTP request, including its entire POST body.
bool readRequest(
    socket_t client,
    string& method,
    string& path,
    string& body,
    int& errorStatus) {

    string request;
    char buffer[4096];

    size_t headerEnd = string::npos;
    size_t contentLength = 0;

    // Read until the complete header is received.
    while (headerEnd == string::npos) {
        int n = recv(client, buffer, sizeof(buffer), 0);

        if (n <= 0) {
            errorStatus = 400;
            return false;
        }

        request.append(buffer, n);

        if (request.size() > MAX_HEADER_SIZE) {
            errorStatus = 431;
            return false;
        }

        headerEnd = request.find("\r\n\r\n");
    }

    string headers = request.substr(0, headerEnd);
    body = request.substr(headerEnd + 4);

    // Parse request line.
    istringstream headerStream(headers);
    string requestLine;

    if (!getline(headerStream, requestLine)) {
        errorStatus = 400;
        return false;
    }

    if (!requestLine.empty() && requestLine.back() == '\r')
        requestLine.pop_back();

    istringstream lineStream(requestLine);
    string httpVersion;

    if (!(lineStream >> method >> path >> httpVersion)) {
        errorStatus = 400;
        return false;
    }

    // Read headers and find Content-Length.
    string header;

    while (getline(headerStream, header)) {
        if (!header.empty() && header.back() == '\r')
            header.pop_back();

        size_t colon = header.find(':');

        if (colon == string::npos)
            continue;

        string name = lowerString(header.substr(0, colon));

        if (name == "transfer-encoding") {
            // This basic server supports Content-Length, not chunked bodies.
            errorStatus = 400;
            return false;
        }

        if (name == "content-length") {
            string value = header.substr(colon + 1);

            size_t first = value.find_first_not_of(" \t");
            size_t last = value.find_last_not_of(" \t");

            if (first == string::npos) {
                errorStatus = 400;
                return false;
            }

            value = value.substr(first, last - first + 1);

            try {
                size_t used = 0;
                unsigned long long length = stoull(value, &used);

                if (used != value.size() ||
                    length > MAX_BODY_SIZE) {
                    errorStatus = 413;
                    return false;
                }

                contentLength = static_cast<size_t>(length);
            }
            catch (...) {
                errorStatus = 400;
                return false;
            }
        }
    }

    // Continue receiving until the complete body arrives.
    while (body.size() < contentLength) {
        int n = recv(client, buffer, sizeof(buffer), 0);

        if (n <= 0) {
            errorStatus = 400;
            return false;
        }

        body.append(buffer, n);

        if (body.size() > MAX_BODY_SIZE) {
            errorStatus = 413;
            return false;
        }
    }

    if (body.size() > contentLength)
        body.resize(contentLength);

    return true;
}

void handleRequest(socket_t client) {
    setSocketTimeout(client);

    string method, path, body;
    int errorStatus = 400;

    if (!readRequest(
        client, method, path, body, errorStatus)) {

        string errorBody = "<h1>Invalid or incomplete HTTP request</h1>";

        if (errorStatus == 413)
            errorBody = "<h1>Request body too large</h1>";
        else if (errorStatus == 431)
            errorBody = "<h1>Request headers too large</h1>";

        sendResponse(client, errorStatus,
            errorStatus == 413 ? "Payload Too Large" :
            errorStatus == 431 ? "Request Header Fields Too Large" :
            "Bad Request", errorBody);

        return;
    }

    // Ignore query strings when checking the route.
    size_t query = path.find('?');

    if (query != string::npos)
        path.resize(query);

    if (path != "/") {
        sendResponse(client, 404, "Not Found",
            "<h1>404 - Page Not Found</h1>");
        return;
    }

    string message;

    if (method == "POST") {
        message = processAction(parseFormBody(body));
    }
    else if (method != "GET") {
        sendResponse(client, 405, "Method Not Allowed",
            "<h1>405 - Method Not Allowed</h1>");
        return;
    }

    string page = renderPage(message);

    sendResponse(client, 200, "OK", page);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "Winsock initialization failed" << endl;
        return 1;
    }
#endif

    const char* envPort = getenv("PORT");

    int port = 8080;

    if (envPort) {
        try {
            size_t used = 0;
            int value = stoi(envPort, &used);

            if (used != string(envPort).size() ||
                value < 1 || value > 65535) {
                cerr << "Invalid PORT environment variable" << endl;
                return 1;
            }

            port = value;
        }
        catch (...) {
            cerr << "Invalid PORT environment variable" << endl;
            return 1;
        }
    }

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);

#ifdef _WIN32
    if (server == INVALID_SOCKET) {
#else
    if (server < 0) {
#endif
        cerr << "Socket creation failed" << endl;
        return 1;
    }

#ifndef _WIN32
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) < 0) {
        cerr << "Bind failed on port " << port << endl;
        closeSocket(server);
        return 1;
    }

    if (listen(server, 128) < 0) {
        cerr << "Listen failed" << endl;
        closeSocket(server);
        return 1;
    }

    cout << "Traffic server listening on port "
         << port << endl;

    while (true) {
        sockaddr_in clientAddress{};

#ifdef _WIN32
        int addressLength = sizeof(clientAddress);
#else
        socklen_t addressLength = sizeof(clientAddress);
#endif

        socket_t client = accept(
            server,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addressLength
        );

#ifdef _WIN32
        if (client == INVALID_SOCKET)
            continue;
#else
        if (client < 0)
            continue;
#endif

        handleRequest(client);
        closeSocket(client);
    }

    closeSocket(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
