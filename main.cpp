#include <iostream>
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>

using namespace std;

class AnonChatClient {
    atomic<bool> connected{false};
    atomic<bool> chat_active{false};
    atomic<bool> running{true};
    mutex cout_mutex;
    int client_socket{-1};

    string get_ip(const char* hostname) {
        addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, "80", &hints, &res) != 0) {
            throw runtime_error("DNS error");
        }

        sockaddr_in* addr = (sockaddr_in*)res->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
        freeaddrinfo(res);

        return ip_str;
    }

    int connect_to_server(const string& ip, int port) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr);

        if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock);
            throw runtime_error("Connection failed");
        }
        return sock;
    }

    void chat_listener() {
        string buffer;
        char temp[1024];
        
        while (running && chat_active) {
            int bytes = recv(client_socket, temp, sizeof(temp), 0);
            if (bytes <= 0) break;
            
            buffer.append(temp, bytes);
            size_t pos;
            while ((pos = buffer.find("\r\n")) != string::npos) {
                string msg = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);
                
                if (!msg.empty()) {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << msg << "\n > " << flush;
                }
            }
        }
    }

public:
    void connect_server() {
        try {
            string host = "192.168.0.111";
            int sock = connect_to_server(get_ip(host.c_str()), 80);
            
            string request = "GET / HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
            send(sock, request.c_str(), request.size(), 0);
            
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\rConnected to server!\n";
            }
            
            connected = true;
            close(sock);
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            connected = true;
        }
    }

    void connect_chat() {
        try {
            string host = "192.168.0.111";
            client_socket = connect_to_server(get_ip(host.c_str()), 8080);
            
            send(client_socket, "CONNECT anonymous\r\n\r\n", 20, 0);
            
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\rConnected to chat!\n > " << flush;
            }
            
            chat_active = true;
            thread(&AnonChatClient::chat_listener, this).detach();
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            if (client_socket != -1) close(client_socket);
        }
    }

    void run() {
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "Welcome to anonchat!\nConnecting to server..." << endl;
        }

        auto connection = async(launch::async, &AnonChatClient::connect_server, this);

        auto start = chrono::steady_clock::now();
        while (!connected) {
            if (chrono::steady_clock::now() - start > chrono::seconds(10)) {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\rConnection timeout!\n";
                return;
            }
            
            for (int i = 0; i < 3; i++) {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\rConnecting" << string(i + 1, '.') << flush;
                this_thread::sleep_for(chrono::milliseconds(300));
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }

        cout << "\nType 'connect' or 'help' to start\n" << endl;

        while (running) {
            string input;
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\n > ";
            }
            getline(cin, input);
            
            if (input == "help") {
                cout << "Commands:\n  connect - Join chat\n  exit    - Quit\n  help    - This help\n";
            }
            else if (input == "exit") {
                running = false;
                cout << "Goodbye!" << endl;
                if (client_socket != -1) close(client_socket);
                break;
            }
            else if (input == "connect" && !chat_active) {
                cout << "Connecting to chat..." << endl;
                connect_chat();
            }
            else if (chat_active) {
                string msg = "MSG " + input + "\r\n";
                if (send(client_socket, msg.c_str(), msg.size(), 0) <= 0) {
                    cerr << "Send failed" << endl;
                    chat_active = false;
                    close(client_socket);
                }
            }
        }
        
        connection.get();
    }
};

int main() {
    AnonChatClient client;
    client.run();
    return 0;
}