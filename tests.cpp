#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>

using namespace std;

void error(string msg) {
    cout << msg << endl;
    exit(0);
}

void send(int sockfd, string msg) {
    msg=msg+"\r\n";
    if(write(sockfd,msg.c_str(), msg.length()) < 0)
        error("Error writing message\n");
}

string receive(int sockfd) {
    char buffer[1024];
    memset(buffer, 0, 1024);
    if(read(sockfd, buffer, 1023) < 0)
        error("Error reading message\n");
    string msg(buffer);
    return msg;
}

int main(int argc, char *argv[]) {
    // check for port
    string av0(argv[0]);
    if (argc < 2)
        error("usage: " + av0 + " serverPort\n");
    int portno = atoi(argv[1]);

    // setup socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    // setup server
    struct sockaddr_in serv_addr, cli_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    // bind to port
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    // listen
    listen(sockfd,5);
    socklen_t clilen = sizeof(cli_addr);
    // accept connection
    // repeat this bit to accept lots of connections
    // you need a new newsockfd for each connection
    int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0)
        error("ERROR on accept");

    bool online = 1;
    string entry;

    while(online) {
        entry = receive(newsockfd);
        cout << entry << endl;
        if(entry == "Exit\r\n"){
            send(newsockfd, "Bye");
            cout << "Enter response: Bye\n";
            break;
        }
        cout << "Enter response: ";
        cin >> entry;
        send(newsockfd, entry);
    }

    close(newsockfd);
    close(sockfd);
    return 0;
}
