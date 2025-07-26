#include <stdio.h>
#include <iostream>
#include <ostream>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <future>
#include <atomic>
#include <mutex>
#include <thread>

using namespace std;
std::atomic<bool> connected(false);
std::mutex cout_mutex;

std::string get_ip(const char* hostname) {
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, "80", &hints, &res) != 0) {
        throw std::runtime_error("DNS error");
    }

    sockaddr_in* addr = (sockaddr_in*)res->ai_addr;
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
    freeaddrinfo(res);

    return ip_str;
}

int connect_to_server(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        throw std::runtime_error("Connection failed");
    }

    return sock;
}

void send_http_request(int sock, const std::string& host, const std::string& post_data = "") {
    std::string request;
    
    if (post_data.empty()) {
        // GET запрос (по умолчанию)
        request = 
            "GET / HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Connection: close\r\n\r\n";
    } else {
        // POST запрос
        request =
            "POST / HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Connection: close\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: " + std::to_string(post_data.size()) + "\r\n\r\n" +
            post_data;
    }

    send(sock, request.c_str(), request.size(), 0);
}

std::string read_http_response(int sock) {
    char buffer[4096];
    std::string response;
    
    while (true) {
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        response.append(buffer, bytes);
    }
    
    return response;
}

void connectServer(std::atomic<bool>& connected)
{
    try {
        std::string host = "example.com";
        std::string ip = get_ip(host.c_str());
        int sock = connect_to_server(ip, 80);
        
        send_http_request(sock, host, "");
        std::string response = read_http_response(sock);
        
        //std::cout << "Ответ сервера:\n" << response << std::endl;
        
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\rConnected!     \n"; 

        connected = true;
        close(sock);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        connected = true;
    }
}

int main() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Welcome to anonchat!\nConnecting to the server..." << std::endl;
    }

    auto result = std::async(std::launch::async, connectServer, std::ref(connected));

    auto start = std::chrono::steady_clock::now();
    while (!connected) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "\rError: connection timeout!\n";
            return 1;
            break; 
        }
        for (int i = 0; i < 3; i++) {
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "\rConnecting" << std::string(i + 1, '.') << std::flush;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\n" << "Type 'connect' or just 'help' to start" << "\n" << std::endl;

    while(connected)
    {
        std::string input;
        std::cout << " > ";
        std::getline(std::cin, input); 
        if(input == "help")
        {
            input = "";
            std::cout << "yay" << std::endl;
        }
        if(input == "exit")
        {
            input = "";
            std::cout << "It was anonchat, bye!" << std::endl;
            break;
        }
        if(input == "connect")
        {
            input = "";
            std::cout << "Searching anon..." << std::endl;
            std::string data = "socket=";
            send_http_request(sock, "example.com/connect", data);
        }
    }

    result.get();
    return 0;
}