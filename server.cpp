#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h> 

#define MaxThread 10
#define BLEN 102400

using namespace std;

void printip(sockaddr_in* ip){
	printf("%d.%d.%d.%d",
  			int(ip->sin_addr.s_addr&0xFF),
  			int((ip->sin_addr.s_addr&0xFF00)>>8),
  			int((ip->sin_addr.s_addr&0xFF0000)>>16),
  			int((ip->sin_addr.s_addr&0xFF000000)>>24));
	return ;
}

bool send_msg(int sockfd, string msg){	//0 for success
	if (send(sockfd,msg.c_str(),msg.size(),0)<0){
		cout<<"Error: Sending msg."<<endl;
		return 1;
	}
	return 0;
}

string recv_msg(int sockfd){
	char buf[BLEN];
	char *bptr=buf;
	int buflen=BLEN;
	string ans="";

	memset(buf, '\0', sizeof(buf)); 

	if (recv(sockfd, bptr, buflen, 0) < 0 ){
		cout<<"Recieving Error!"<<endl;
	}
	string tmp(buf);
	ans=tmp;
	return ans;
}

bool server_socket(int port){	//0 for success
	int s_socket,connectfd;
	struct sockaddr_in serv_addr,clin_addr;

	if ((s_socket = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
		cout<<"Could not create socket"<<endl;
		return 1;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port); 
	
    if (bind(s_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
    	cout<<"Binding Error!!"<<endl;
    	return 1;
    }

    if (listen(s_socket,15) != 0){
    	cout<<"Listening Error!!"<<endl;
    	return 1;
    }

    socklen_t client_len = sizeof(clin_addr);
    connectfd = accept(s_socket, (struct sockaddr *) &clin_addr, &client_len);
    if (connectfd < 0){
        cout<<"ERROR on accept"<<endl;
        return 1;
    }

    printip(&clin_addr);
    cout<<endl;
    string entry;

    while(1) {
        entry = recv_msg(connectfd);
        cout << entry << endl;
        if(entry == "Exit"){
            send_msg(connectfd, "Bye");
            cout << "Enter response: Bye";
            break;
        }
        cout << "Enter response: ";
        cin >> entry;
        send_msg(connectfd, entry);
    }

    close(connectfd);
    close(s_socket);
    return 0;

}

int main(int argc, char const *argv[]){
	string arg0(argv[0]);
	if (argc != 2){
		cout<<"Usage : "<<arg0<<" listenport"<<endl;
		exit(0);
	}

	int listenport = atoi(argv[1]);
	if((listenport > 65535) || (listenport < 2000))
    {
        cout << "Please enter a port number between 2000 - 65535" << endl;
        return 0;
    }

	server_socket(listenport);

	return 0;
}