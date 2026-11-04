#include <algorithm>
#include <cstdlib>
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

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#endif

using namespace std;

const size_t MAXIMUM_QUEUE_CAPACITY = 20;

string htmlEscape(const string& text)
{
    string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
        case '&':  out += "&amp;";  break;
        case '<':  out += "&lt;";   break;
        case '>':  out += "&gt;";   break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&#39;";  break;
        default:   out += c;
        }
    }
    return out;
}

optional<int> toInt(const string& text)
{
    try
    {
        size_t used = 0;
        int value = stoi(text, &used);
        if (used != text.size()) return nullopt;
        return value;
    }
    catch (...)
    {
        return nullopt;
    }
}

struct Vehicle
{
    string registrationNumber;
    string vehicleType;
};

struct Road
{
    int destinationIntersectionId;
    string roadName;
    bool isBlocked = false;
    deque<Vehicle> waitingQueue;

    bool enqueueVehicle(Vehicle v)
    {
        if (waitingQueue.size() >= MAXIMUM_QUEUE_CAPACITY) return false;
        waitingQueue.push_back(std::move(v));
        return true;
    }

    string queueHtml() const
    {
        if (waitingQueue.empty()) return "<em>Queue is empty</em>";
        ostringstream html;
        html << "<ul>";
        for (const Vehicle& v : waitingQueue)
            html << "<li>" << htmlEscape(v.registrationNumber)
                 << " (" << htmlEscape(v.vehicleType) << ")</li>";
        html << "</ul>";
        return html.str();
    }
};

struct Intersection
{
    int intersectionId;
    string intersectionName;
    list<Road> roads;

    Intersection(int id, string name)
        : intersectionId(id), intersectionName(std::move(name))
    {
    }

    Road* findRoad(int destId)
    {
        auto it = find_if(roads.begin(), roads.end(),
            [destId](const Road& r) { return r.destinationIntersectionId == destId; });
        return it == roads.end() ? nullptr : &*it;
    }

    Road* findRoadByName(const string& name)
    {
        auto it = find_if(roads.begin(), roads.end(),
            [&name](const Road& r) { return r.roadName == name; });
        return it == roads.end() ? nullptr : &*it;
    }

    bool hasRoadNamed(const string& name) const
    {
        return any_of(roads.begin(), roads.end(),
            [&name](const Road& r) { return r.roadName == name; });
    }

    bool hasRoadTo(int destId) const
    {
        return any_of(roads.begin(), roads.end(),
            [destId](const Road& r) { return r.destinationIntersectionId == destId; });
    }

    bool setRoadBlocked(int destId, bool blocked)
    {
        Road* r = findRoad(destId);
        if (!r) return false;
        r->isBlocked = blocked;
        return true;
    }

    size_t totalWaiting() const
    {
        size_t n = 0;
        for (const Road& r : roads) n += r.waitingQueue.size();
        return n;
    }

    string roadsHtml() const
    {
        if (roads.empty()) return "<em>No outgoing roads</em>";
        ostringstream html;
        html << "<ul>";
        for (const Road& r : roads)
        {
            html << "<li>To ID: " << r.destinationIntersectionId
                 << " via " << htmlEscape(r.roadName)
                 << (r.isBlocked
                     ? " <strong style='color:red;'>[BLOCKED]</strong>"
                     : " <strong style='color:green;'>[OPEN]</strong>")
                 << "<br><small>Waiting (" << r.waitingQueue.size() << "):</small>"
                 << r.queueHtml()
                 << "</li>";
        }
        html << "</ul>";
        return html.str();
    }
};

class CityTrafficNetwork
{
private:
    map<int, Intersection> intersections;

    Intersection* find(int id)
    {
        auto it = intersections.find(id);
        return it == intersections.end() ? nullptr : &it->second;
    }

public:
    string addIntersection(int newId, const string& newName)
    {
        if (!intersections.emplace(newId, Intersection(newId, newName)).second)
            return "Intersection ID " + to_string(newId) + " already exists.";
        return "Intersection '" + htmlEscape(newName) + "' (ID: " + to_string(newId) + ") added successfully.";
    }

    string connectIntersections(int firstId, int secondId, const string& roadName)
    {
        Intersection* a = find(firstId);
        Intersection* b = find(secondId);
        if (!a || !b) return "Invalid intersection IDs provided.";
        if (firstId == secondId) return "Cannot connect an intersection to itself.";
        if (a->hasRoadTo(secondId)) return "A road already exists between these intersections.";
        if (a->hasRoadNamed(roadName) || b->hasRoadNamed(roadName))
            return "Road name '" + htmlEscape(roadName) + "' is already used at one of these intersections. "
                   "Road names must be unique per intersection.";

        a->roads.push_front(Road{ secondId, roadName, false, {} });
        b->roads.push_front(Road{ firstId, roadName, false, {} });
        return "Road '" + htmlEscape(roadName) + "' connected between Intersection " +
               to_string(firstId) + " and " + to_string(secondId) + ".";
    }

    string addVehicleToRoad(int id, const string& roadName, const string& vehicleNumber, const string& vehicleType)
    {
        Intersection* node = find(id);
        if (!node) return "Intersection ID " + to_string(id) + " not found.";
        Road* road = node->findRoadByName(roadName);
        if (!road) return "No road named '" + htmlEscape(roadName) + "' at intersection " + to_string(id) + ".";

        if (road->enqueueVehicle({ vehicleNumber, vehicleType }))
            return "Vehicle " + htmlEscape(vehicleNumber) + " (" + htmlEscape(vehicleType) +
                   ") queued at " + htmlEscape(node->intersectionName) + " for road '" +
                   htmlEscape(road->roadName) + "'.";
        return "Queue is full on road '" + htmlEscape(road->roadName) + "'.";
    }

    string allowVehiclesToPass(int id, const string& roadName, int count)
    {
        Intersection* node = find(id);
        if (!node) return "Intersection ID " + to_string(id) + " not found.";
        Road* road = node->findRoadByName(roadName);
        if (!road) return "No road named '" + htmlEscape(roadName) + "' at intersection " + to_string(id) + ".";
        if (count <= 0) return "Number of vehicles must be positive.";
        if (road->isBlocked)
            return "Road '" + htmlEscape(road->roadName) + "' is BLOCKED. No vehicles can pass.";

        size_t passed = min(static_cast<size_t>(count), road->waitingQueue.size());
        road->waitingQueue.erase(road->waitingQueue.begin(),
                                 road->waitingQueue.begin() + passed);

        return "Signal GREEN at " + htmlEscape(node->intersectionName) + " for road '" +
               htmlEscape(road->roadName) + "'. " + to_string(passed) +
               " vehicle(s) passed. Remaining: " + to_string(road->waitingQueue.size()) + ".";
    }

    string changeRoadBlockStatus(int firstId, int secondId, bool blocked)
    {
        Intersection* a = find(firstId);
        Intersection* b = find(secondId);
        if (!a || !b) return "Invalid intersection IDs.";

        bool fwd = a->setRoadBlocked(secondId, blocked);
        bool rev = b->setRoadBlocked(firstId, blocked);
        if (!(fwd && rev)) return "Road does not exist between these two intersections.";

        return "Road between " + to_string(firstId) + " and " + to_string(secondId) +
               (blocked ? " is now BLOCKED." : " is now REOPENED.");
    }

    string getNetworkHtml() const
    {
        if (intersections.empty())
            return "<p>No intersections registered in the network.</p>";

        ostringstream html;
        for (const auto& [id, node] : intersections)
        {
            html << "<div style='background:#f4f4f4; padding:12px; margin-bottom:12px; "
                    "border-radius:6px; border:1px solid #ccc;'>"
                 << "<h3>Intersection [" << id << "] " << htmlEscape(node.intersectionName) << "</h3>"
                 << "<p><strong>Total waiting vehicles: " << node.totalWaiting() << "</strong></p>"
                 << "<p><strong>Roads &amp; their queues:</strong></p>"
                 << node.roadsHtml()
                 << "</div>";
        }
        return html.str();
    }
};

CityTrafficNetwork simulator;

string urlDecode(const string& text)
{
    string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '+')
        {
            result += ' ';
        }
        else if (text[i] == '%' && i + 2 < text.size())
        {
            try
            {
                result += static_cast<char>(stoi(text.substr(i + 1, 2), nullptr, 16));
                i += 2;
            }
            catch (...)
            {
                result += text[i];
            }
        }
        else
        {
            result += text[i];
        }
    }
    return result;
}

map<string, string> parseFormBody(const string& postData)
{
    map<string, string> parsed;
    istringstream stream(postData);
    string pair;
    while (getline(stream, pair, '&'))
    {
        size_t eq = pair.find('=');
        if (eq != string::npos)
            parsed[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }
    return parsed;
}

void replaceFirst(string& text, const string& token, const string& replacement)
{
    size_t pos = text.find(token);
    if (pos != string::npos) text.replace(pos, token.size(), replacement);
}

string renderPage(const string& message)
{
    ifstream file("templates/index.html");
    string content = file.is_open()
        ? string(istreambuf_iterator<char>(file), istreambuf_iterator<char>())
        : "<h1>Error: templates/index.html not found</h1>";

    string messageHtml;
    if (!message.empty())
        messageHtml = "<div style='padding:10px; background:#e0f2fe; border:1px solid #0284c7; "
                      "color:#0369a1; border-radius:4px; margin-bottom:15px; font-weight:bold;'>" +
                      message + "</div>";

    replaceFirst(content, "{{MESSAGE}}", messageHtml);
    replaceFirst(content, "{{NETWORK}}", simulator.getNetworkHtml());
    return content;
}

string processAction(const map<string, string>& form)
{
    auto get = [&](const string& key) -> string
    {
        auto it = form.find(key);
        return it == form.end() ? "" : it->second;
    };

    const string action = get("action");

    if (action == "add_intersection")
    {
        auto id = toInt(get("id"));
        if (id && !get("name").empty())
            return simulator.addIntersection(*id, get("name"));
    }
    else if (action == "connect_road")
    {
        auto src = toInt(get("src")), dest = toInt(get("dest"));
        if (src && dest && !get("name").empty())
            return simulator.connectIntersections(*src, *dest, get("name"));
    }
    else if (action == "add_vehicle")
    {
        auto id = toInt(get("id"));
        if (id && !get("road").empty() && !get("reg").empty() && !get("type").empty())
            return simulator.addVehicleToRoad(*id, get("road"), get("reg"), get("type"));
    }
    else if (action == "green_signal")
    {
        auto id = toInt(get("id")), count = toInt(get("count"));
        if (id && !get("road").empty() && count)
            return simulator.allowVehiclesToPass(*id, get("road"), *count);
    }
    else if (action == "block_road" || action == "reopen_road")
    {
        auto src = toInt(get("src")), dest = toInt(get("dest"));
        if (src && dest)
            return simulator.changeRoadBlockStatus(*src, *dest, action == "block_road");
    }
    return "";
}

void closeSocket(socket_t s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

void handle_request(socket_t client_socket, const string& method, const string& post_data)
{
    string statusMessage;
    if (method == "POST")
        statusMessage = processAction(parseFormBody(post_data));

    string pageHtml = renderPage(statusMessage);
    string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: " +
                      to_string(pageHtml.size()) + "\r\n\r\n" + pageHtml;

    send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
    closeSocket(client_socket);
}

pair<string, string> parse_request(const string& request_data)
{
    istringstream stream(request_data);
    string line;
    getline(stream, line);

    istringstream line_stream(line);
    vector<string> tokens{ istream_iterator<string>(line_stream), istream_iterator<string>() };

    string method = tokens.size() > 0 ? tokens[0] : "";
    string path = tokens.size() > 1 ? tokens[1] : "";
    return { method, path };
}

void run_server()
{
    const char* host = "0.0.0.0";
    int port = 8080;

    const char* env_port = getenv("PORT");
    if (env_port != nullptr)
    {
        port = atoi(env_port);
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        cerr << "Failed to initialize Winsock" << endl;
        return;
    }
    socket_t server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
#else
    socket_t server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
#endif
    {
        cerr << "Failed to create socket" << endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

#ifndef _WIN32
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_address{};
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(host);
    server_address.sin_port = htons(port);

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) < 0 ||
        listen(server_socket, 10) < 0)
    {
        cerr << "Failed to bind/listen on port " << port << endl;
        closeSocket(server_socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    cout << "Server listening on http://" << host << ":" << port << endl;

    while (true)
    {
        sockaddr_in client_address{};
#ifdef _WIN32
        int client_address_size = sizeof(client_address);
#else
        socklen_t client_address_size = sizeof(client_address);
#endif
        socket_t client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_size);
#ifdef _WIN32
        if (client_socket == INVALID_SOCKET) continue;
#else
        if (client_socket == -1) continue;
#endif

        string request_data;
        char buffer[4096];
        int content_length = -1;
        size_t header_end = string::npos;

        while (true)
        {
            int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;

            request_data.append(buffer, bytes);

            if (header_end == string::npos)
            {
                header_end = request_data.find("\r\n\r\n");
                if (header_end != string::npos)
                {
                    string lower = request_data;
                    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    size_t cl_pos = lower.find("content-length:");
                    if (cl_pos != string::npos && cl_pos < header_end)
                    {
                        size_t end_line = lower.find("\r\n", cl_pos);
                        content_length = stoi(lower.substr(cl_pos + 15, end_line - (cl_pos + 15)));
                    }
                    else
                    {
                        content_length = 0;
                    }
                }
            }

            if (header_end != string::npos)
            {
                size_t body_received = request_data.size() - (header_end + 4);
                if (content_length >= 0 && body_received >= static_cast<size_t>(content_length))
                {
                    break;
                }
            }
        }

        if (request_data.empty())
        {
            closeSocket(client_socket);
            continue;
        }

        string method = parse_request(request_data).first;
        string post_data;

        if (method == "POST" && header_end != string::npos)
        {
            post_data = request_data.substr(header_end + 4);
        }

        handle_request(client_socket, method, post_data);
    }
}

int main()
{
    run_server();
    return 0;
}
