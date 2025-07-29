#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm> 
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <ctime>

using namespace std;

vector<int> clients;
mutex clients_mutex;

void handle_client(int client_socket) {
    char buffer[1024];
    srand(time(0));
    string client_name = "Guest_" + std::to_string(rand() % 10000);
    //string client_name = "Anonymous";

    try {
        
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) throw runtime_error("Connection closed");

        string message(buffer, bytes);
        if (message.find("CONNECT") == 0) {
            
            size_t space_pos = message.find(' ');
            if (space_pos != string::npos) {
                client_name = message.substr(space_pos + 1);
                client_name.erase(client_name.find_last_not_of(" \r\n") + 1);
            }

            
            string welcome = "Welcome to chat, " + client_name + "!\n";
            send(client_socket, welcome.c_str(), welcome.size(), 0);
        } else {
            throw runtime_error("Protocol error: expected CONNECT");
        }

        
        while (true) {
            bytes = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;

            string msg(buffer, bytes);
            if (msg.find("MSG ") == 0) {
                string broadcast = client_name + ": " + msg.substr(4);

                
                lock_guard<mutex> lock(clients_mutex);
                for (int client : clients) {
                    if (client != client_socket) {
                        send(client, broadcast.c_str(), broadcast.size(), 0);
                    }
                }
            }
        }
    } catch (const exception& e) {
        cerr << "Client error: " << e.what() << endl;
    }

    
    {
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(remove(clients.begin(), clients.end(), client_socket), clients.end());
    }
    close(client_socket);
    cout << client_name << " disconnected" << endl;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        cerr << "Socket creation failed" << endl;
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Bind failed" << endl;
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        cerr << "Listen failed" << endl;
        return 1;
    }

    cout << "Server started on port 8080" << endl;

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            cerr << "Accept failed" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        cout << "New connection from " << client_ip << endl;

        {
            lock_guard<mutex> lock(clients_mutex);
            clients.push_back(client_socket);
        }

        thread(handle_client, client_socket).detach();
    }

    close(server_socket);
    return 0;
}