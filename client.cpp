#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstdlib>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace std;



bool client_socket(const char* ip, int port){
	struct hostent* hptr;
	struct sockaddr_in serv_addr;
	int c_socket;
	//cout<<"func:"<<ip<<' '<<port<<' '<<endl;
	if (hptr = gethostbyname(ip)){
		if (c_socket= socket(AF_INET,SOCK_STREAM,0) < 0){
			cout<<"Could not create socket"<<endl;
			return 1;
		}

		memset(&serv_addr, 0, sizeof(serv_addr)); 
    	serv_addr.sin_family = AF_INET;
    	serv_addr.sin_port = htons(port);

    	if (connect(c_socket,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    		cout<<"Could not connect to host"<<endl;
			return 1;
    	}

    	//finish connection;
	}
	else {
		cout<<"Could not find hostip"<<endl;
		return 1;
	}

}

int main(int argc, char const *argv[])
{
	string arg0(argv[0]);
	if (argc != 3){
		cout<<"Usage : "<<arg0<<" hostip hostport"<<endl;
		exit(0);
	}
	const char* hostip = argv[1];
	int hostport = atoi(argv[2]);

	//cout<<"test:"<<arg0<<' '<<hostip<<' '<<hostport<<endl;

	client_socket(hostip,hostport);

	return 0;
}