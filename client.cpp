#include <iostream>
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

#define BLEN 1024

using namespace std;

bool action(string req,int sockfd){	//0 for success
	string cmd=""
	string ans[1005];
	int findpos;

	if ((findpos=req.find("#")) >= 0){
		cmd = req.substr(0,findpos);
		req = req.substr(findpos,req.size()-findpos);
	}
	else {
		cmd = req;
	}

	if (cmd=="Exit"){
		ans[0]=recv(sockfd);
		cout<<ans[0]<<endl;
		return 0;
	}
	else if (cmd==""){

	}
}

bool send_msg(int sockfd, string msg){	//0 for success
	msg=msg+"\r\n";
	if (send(sockfd,msg.c_str(),msg.size(),0)<0){
		cout<<"Error: Sending msg."<<endl;
		return 1;
	}
	return 0;
}

string recv_msg(int sockfd){
	char buf[BLEN];
	char *bptr=buf;
	int n=0;
	int buflen=BLEN;
	string ans="";

	memset(buf, '\0', sizeof(buf)); 

	while ((n = recv(sockfd, bptr, buflen, 0)) > 0) {
		bptr+= n;
		buflen-= n;
		if (buflen<=0){
			string tmp(buf);
			ans=ans+tmp;
			bptr-=BLEN;
			buflen+=BLEN;
		}
		if (*(bptr-1)=='\n' && *(bptr-2)=='\r') break;
	}
	string tmp(buf);
	ans=ans+tmp.substr(0,tmp.size()-2);
	return ans;
}

bool client_socket(const char* ip, int port){	//0 for success
	struct hostent* hptr;
	struct sockaddr_in serv_addr;
	int c_socket;
	
	if (hptr = gethostbyname(ip)){
		if ((c_socket = socket(AF_INET,SOCK_STREAM,0)) < 0){
			cout<<"Could not create socket"<<endl;
			return 1;
		}

		memset(&serv_addr, '0', sizeof(serv_addr)); 
    	serv_addr.sin_family = AF_INET;
    	memmove((char *)&serv_addr.sin_addr.s_addr,(char *)hptr->h_addr,hptr->h_length);
    	serv_addr.sin_port = htons(port);

    	if (connect(c_socket,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    		cout<<"Could not connect to host"<<endl;
			return 1;
    	}
    	cout<<"Connection accepted"<<endl;
    	//finish connection
    	
    	string req;
    	string ans="test";

    	cout<<"Enter your message:"<<endl;
    	while (getline(cin,req)){
    		send_msg(c_socket,req);
    		sleep(0.25);
    		cout<<"Output:"<<endl;
    		action(req,c_socket);
    		if (req=="Exit"){
    			close(c_socket);
    			cout<<"Logout : by user request."<<endl;
    			break;
    		}
    		cout<<"Enter your message:"<<endl;
    	}

    	return 0;
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

	client_socket(hostip,hostport);

	return 0;
}