#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <stdio.h>
#include <arpa/inet.h>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <map>
#include <vector>
#include <deque>
#include <cstring>
#include <iterator>

#define MAX_CLIENTS 2000
#define BUFFER_SIZE 1024
#define WELCOME_MSG "*********************************\n** Welcome to the Chat server. **\n*********************************\n"

struct client_info
{
    std::string passwd;
    std::string status;
    int room_number;
    int sd;
    bool logged_in = false;
};

struct chat_room_info
{
    std::string owner;
    std::string pin_message="";
    std::vector<std::string> members;
    std::deque<std::pair<std::string, std::string>> history;
    bool operator=(const chat_room_info& other) const {
        return owner == other.owner && pin_message == other.pin_message && members == other.members && history == other.history;
    }
};

std::map<int, chat_room_info> chat_rooms;
std::map<std::string, client_info> client_list;

std::vector<std::string> forbidden_words = {"==", "superpie", "hello", "starburst stream", "domain expansion"};

std::string filter_message(const std::string& message) {
    std::string filtered_message = message;
    for (const auto& word : forbidden_words) {
        size_t start_pos = 0;
        while((start_pos = filtered_message.find(word, start_pos)) != std::string::npos) {
            filtered_message.replace(start_pos, word.length(), std::string(word.length(), '*'));
            start_pos += word.length(); // Handles overlapping words
        }
    }
    return filtered_message;
}

std::string get_chat_history(int number) {
    std::string history;
    if (chat_rooms.count(number) > 0) {
        for (const auto& record : chat_rooms[number].history) {
            history += record.second + "\n";
        }
    }
    return history;
}

void add_message_to_history(int number, const std::string& username, const std::string& message) {
    if (chat_rooms[number].history.size() == 10) {
        if (chat_rooms[number].history.front().second == chat_rooms[number].pin_message) {
            chat_rooms[number].pin_message.clear();
        }
        chat_rooms[number].history.pop_front();
    }
    chat_rooms[number].history.push_back({username, message});
}

void send_message_to_chat_room(int number, const std::string& message, const std::string& sender) {
    for (const auto& username : chat_rooms[number].members) {
        if(username != sender)
            send(client_list[username].sd, message.c_str(), message.size(), 0);
    }
}

bool process_command(int sd, const std::string& cmd) {
    if(cmd.size() == 0){
        send(sd, "% ", 2, 0);
        return true;
    }
    std::string response="";
    std::istringstream iss(cmd);
    std::vector<std::string> tokens(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
    std::string command = tokens[0];
    // for (const auto& token : tokens) {
    //     printf("%d Token: %s\n", sd, token.c_str());
    // }

    auto it = std::find_if(client_list.begin(), client_list.end(), [sd](const auto& pair) { return pair.second.sd == sd; });

    // Check if the client is in the chat room
    if (it != client_list.end() && it->second.room_number != -1) {
        // Chatroom command
        if (command == "/pin") {
            std::string message = cmd.substr(5);
            if (message.length() > 150) {
                response = "Error: Message length cannot exceed 150 characters.\n";
            } else {
                // Set the pin message in the chat room
                std::string pin_message = "Pin -> [" + it->first + "]: " + message;
                if (!chat_rooms[it->second.room_number].pin_message.empty()){
                    std::string old_pin_message = chat_rooms[it->second.room_number].pin_message;
                    auto at = std::find_if(chat_rooms[it->second.room_number].history.begin(), chat_rooms[it->second.room_number].history.end(), [old_pin_message](const auto& pair) { return pair.second == old_pin_message; });
                    if(at != chat_rooms[it->second.room_number].history.end())
                        chat_rooms[it->second.room_number].history.erase(at);
                }
                pin_message = filter_message(pin_message);
                chat_rooms[it->second.room_number].pin_message = pin_message;
                // Send the pin message to all users in the chat room
                add_message_to_history(it->second.room_number, it->first, pin_message);
                send_message_to_chat_room(it->second.room_number, pin_message+"\n", it->first);
                response = pin_message + "\n";
            }
        }
        else if (command == "/delete-pin") {
            // Check if there is a pin message in the chat room
            if (chat_rooms[it->second.room_number].pin_message.empty()) {
                response = "No pin message in chat room " + std::to_string(it->second.room_number) + "\n";
            } else {
                // Delete the pin message in the chat room
                if (!chat_rooms[it->second.room_number].pin_message.empty()){
                    std::string old_pin_message = chat_rooms[it->second.room_number].pin_message;
                    auto at = std::find_if(chat_rooms[it->second.room_number].history.begin(), chat_rooms[it->second.room_number].history.end(), [old_pin_message](const auto& pair) { return pair.second == old_pin_message; });
                    if(at != chat_rooms[it->second.room_number].history.end())
                        chat_rooms[it->second.room_number].history.erase(at);
                }
                chat_rooms[it->second.room_number].pin_message.clear();
                //response = "Pin message deleted.\n";
            }
        }
        else if (command == "/exit-chat-room") {
            // Remove the user from the chat room
            int room_number = it->second.room_number;
            it->second.room_number = -1;
            chat_rooms[room_number].members.erase(std::remove(chat_rooms[room_number].members.begin(), chat_rooms[room_number].members.end(), it->first), chat_rooms[room_number].members.end());
            // Send a message to all clients in the chat room that the user has left
            std::string exit_message = it->first + " had left the chat room.\n";
            for (const auto& client : client_list) {
                if (client.second.room_number == room_number) {
                    send(client.second.sd, exit_message.c_str(), exit_message.size(), 0);
                }
            }

            //response = "You have left the chat room.\n";
        }
        else if (command == "/list-user") {
            // Iterate over all clients in the chat room and append their usernames and statuses to the response
            for (const auto& client : client_list) {
                if (client.second.room_number == it->second.room_number) {
                    response += client.first + " " + client.second.status + "\n";
                }
            }
        }
        else if (command[0] == '/'){
            response = "Error: Unknown command\n";
        }
        else {
            // Send the message to all clients in the chat room
            add_message_to_history(it->second.room_number, it->first, "[" + it->first + "]: " + filter_message(cmd));
            send_message_to_chat_room(it->second.room_number, "[" + it->first + "]: " + filter_message(cmd) + "\n", it->first);
            response = "[" + it->first + "]: " + filter_message(cmd) + "\n";
        }
    }
    // Default Command
    else if (command == "register") {
        std::string username, password;
        if (tokens.size() != 3) {
            response = "Usage: register <username> <password>\n";
        } else {
            username = tokens[1];
            password = tokens[2];
            if (client_list.find(username) == client_list.end()) {
                client_list[username] = {password, "offline", -1, -1, false};
                //printf("Register: %s %s\n", username.c_str(), password.c_str());
                response = "Register successfully.\n";
            } else {
                response = "Username is already used.\n";
            }
        }
    } 
    else if (command == "login") {
        std::string username, password;
        if (tokens.size() != 3) {
            response = "Usage: login <username> <password>\n";
        } else {
            username = tokens[1];
            password = tokens[2];
            //printf("Login: %s %s\n", username.c_str(), password.c_str());
            if (client_list.find(username) == client_list.end()) {
                response = "Login failed.\n";
            } 
            else if(it->second.logged_in){
                response = "Please logout first.\n";
            }
            else if (client_list[username].passwd != password) {
                response = "Login failed.\n";
            } 
            else {
                client_list[username].logged_in = true;
                client_list[username].sd = sd;
                client_list[username].status = "online";
                response = "Welcome, " + username + ".\n";
            }
        }
    }
    else if (command == "logout") {
        if (tokens.size() != 1) {
            response = "Usage: logout\n";
        } else {
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Log out the user
                std::string username = it->first;
                it->second.status = "offline";
                it->second.sd = -1;
                response = "Bye, " + username + ".\n";
            }
        }
    }
    else if (command == "whoami") {
        if (tokens.size() != 1) { 
            response = "Usage: whoami\n";
        } else {
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Get the username
                std::string username = it->first;
                response = username + "\n";
            }
        }
    }
    else if (command == "exit") {
        if (tokens.size() != 1) {
            response = "Usage: exit\n";
        } else {
            // Check if the client is logged in
            if (it != client_list.end()) {
                // Log out the user
                std::string username = it->first;
                it->second.status = "offline";
                it->second.sd = -1;
                send(sd, ("Bye, " + username + ".\n").c_str(), ("Bye, " + username + ".\n").size(), 0);
            }
            close(sd);
            return false;
        }
    }
    else if (command == "set-status") {
        std::string status;
        if (tokens.size() != 2) {
            response = "Usage: set-status <status>\n";
        } 
        else {
            status = tokens[1];
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Check if the status is valid
                if (status != "online" && status != "offline" && status != "busy") {
                    response = "set-status failed\n";
                } else {
                    // Set the status
                    std::string username = it->first;
                    it->second.status = status;
                    response = username + " " + status + "\n";
                }
            }
        }
    }
    else if (command == "list-user") {
        if (tokens.size() != 1) {
            response = "Usage: list-user\n";
        } else {
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Get all users and their statuses
                std::vector<std::pair<std::string, std::string>> users;
                for (const auto& pair : client_list) {
                    users.push_back({pair.first, pair.second.status});
                }
                // Sort the users by username
                std::sort(users.begin(), users.end());
                // Format the response
                for (const auto& pair : users) {
                    response += pair.first + " " + pair.second + "\n";
                }
            }
        }
    }
    else if (command == "enter-chat-room") {
        int number;
        if (tokens.size() != 2) {
            response = "Usage: enter-chat-room <number>\n";
        } else if (atoi(tokens[1].c_str()) < 1 || atoi(tokens[1].c_str()) > 100) {
            number = atoi(tokens[1].c_str());
            response = "Number " + std::to_string(number) + " is not valid.\n";
        } else {
            number = atoi(tokens[1].c_str());
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Enter the chat room
                std::string username = it->first;
                it->second.room_number = number;
                if(chat_rooms[number].owner == "")
                    chat_rooms[number].owner = username;
                if(std::find(chat_rooms[number].members.begin(), chat_rooms[number].members.end(), username) == chat_rooms[number].members.end())
                    chat_rooms[number].members.push_back(username);
                response = "Welcome to the public chat room.\nRoom number: " + std::to_string(number) + "\nOwner: " + chat_rooms[number].owner + "\n";
                response += get_chat_history(number);
                send_message_to_chat_room(number, username + " had enter the chat room.\n", username);
            }
        }
    }
    else if (command == "list-chat-room") {
        if (tokens.size() != 1) {
            response = "Usage: list-chat-room\n";
        } else {
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // List all chat rooms and their owners
                for (const auto& pair : chat_rooms) {
                    //printf("%d %s\n", pair.first, pair.second.owner.c_str());
                    response += pair.second.owner + " " + std::to_string(pair.first) + "\n";
                }
            }
        }
    }
    else if (command == "close-chat-room") {
        int number;
        if (tokens.size() != 2) {
            response = "Usage: close-chat-room <number>\n";
        }
        else if (atoi(tokens[1].c_str()) < 1 || atoi(tokens[1].c_str()) > 100) {
            number = atoi(tokens[1].c_str());
            response = "Number " + std::to_string(number) + " is not valid.\n";
        } 
        else {
            number = atoi(tokens[1].c_str());
            // Check if the client is logged in
            if (it == client_list.end()) {
                response = "Please login first.\n";
            } else {
                // Check if the chat room exists
                if (chat_rooms.count(number) == 0) {
                    response = "Chat room " + std::to_string(number) + " does not exist.\n";
                } else {
                    // Check if the user is the owner
                    if (chat_rooms[number].owner != it->first) {
                        response = "Only the owner can close this chat room.\n";
                    } else {
                        // Close the chat room
                        response = "Chat room " + std::to_string(number) + " was closed.\n";
                        for (const auto& id : chat_rooms[number].members) {
                            if(client_list[id].sd != -1 && client_list[id].room_number != -1)
                                send(client_list[id].sd, (response+"% ").c_str(), (response+"% ").size(), 0);
                            client_list[id].room_number = -1;
                        }
                        send(sd, response.c_str(), response.size(), 0);
                        chat_rooms.erase(number);
                    }
                }
            }
        }
    }
    else {
        response = "Error: Unknown command\n";
    }
    if(response.size() > 0)
        send(sd, response.c_str(), response.size(), 0);
    if(it == client_list.end() || it->second.room_number == -1)
        send(sd, "% ", 2, 0);
    return true;
}
    

int main(int argc, char *argv[]) {
    // Parse command line arguments to get the port number
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [port number]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port_number = atoi(argv[1]);
    int server_socket, client_socket, max_sd, sd, activity;
    int client_socket_arr[MAX_CLIENTS] = {0};
    struct sockaddr_in address;
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    // Create server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

    // Set server details
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_number);

    // Bind server socket to port
    if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (true) {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add server socket to set
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket_arr[i];

            // If valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            // Highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        // Wait for an activity on one of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        // If something happened on the server socket, then its an incoming connection
        if (FD_ISSET(server_socket, &readfds)) {
            int clen = sizeof(address);
            if ((client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&clen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // Send new connection welcome message
            if (send(client_socket, WELCOME_MSG, strlen(WELCOME_MSG), 0) != strlen(WELCOME_MSG)) {
                perror("send");
            }
            
            send(client_socket, "% ", 2, 0);

            // Add new socket to array of sockets
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socket_arr[i] == 0) {
                    client_socket_arr[i] = client_socket;
                    break;
                }
            }
        }

        // Else its some IO operation on some other socket
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket_arr[i];
            if (FD_ISSET(sd, &readfds)) {
                // Check if it was for closing, and also read the incoming message
                int valread = read(sd, buffer, 1024);
                if (valread == 0) {
                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket_arr[i] = 0;
                } else {
                    std::string message = std::string(buffer, valread);
                    message.pop_back();
                    //printf("%d Message: %s\n", sd, message.c_str());
                    if(!process_command(sd, message)){
                        close(sd);
                        client_socket_arr[i] = 0;
                        break;
                    }
                }
            }
        }
    }

    return 0;
}