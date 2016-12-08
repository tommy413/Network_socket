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

#define BLEN 102400

using namespace std;

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

bool display_option(string* options){ //0 for success
	int count=atoi(options[0].c_str());
	cout<<"Choose Request:"<<endl;
	for (int i = 1; i <= count; ++i)
	{
		cout<<i<<". "<<options[i]<<endl;
	}
	cout<<"(Please enter the corresponding request index.)"<<endl;
	return 0;
}

string make_req(string req,string* options){
	int choice=atoi(req.c_str());
	cout<<endl;
	if (choice > atoi(options[0].c_str()) || choice <= 0){
		cout<<"No corresponding request!!!"<<endl;
		return "error";
	}
	string act=options[choice];
	string args[10];

	if (act=="Register"){
		cout<<"Account Name?"<<endl;
		getline(cin,args[0]);
		return "REGISTER#"+args[0];
	}
	else if (act=="Login"){
		cout<<"Account Name?"<<endl;
		getline(cin,args[0]);
		cout<<"Port Number?"<<endl;
		getline(cin,args[1]);
		return args[0]+"#"+args[1];
	}
	else if (act=="List"){
		return act;
	}
	else if (act=="Logout"){
		return "Exit";
	}
}

bool action(string req,int sockfd){	//0 for success
	string cmd="";
	string ans;
	int findpos;

	if ((findpos=req.find("#")) >= 0){
		cmd = req.substr(0,findpos);
		req = req.substr(findpos+1,req.size()-findpos);
	}
	else {
		cmd = req;	
	}

	if (cmd=="Exit"){
		ans=recv_msg(sockfd);
		cout<<ans<<endl;
		if (ans!="bye") return 1;
		return 0;
	}
	else if (cmd=="REGISTER"){
		ans=recv_msg(sockfd);
		cout<<ans<<endl;
		if (ans!="100 OK\n") return 1;
		return 0;
	}
	else if (cmd=="List"){
		ans=recv_msg(sockfd);
		if (ans[0]=='2'){
			cout<<ans<<endl<<endl;
			return 1;
		}
		for (int i = 0; i < ans.size(); ++i)
		{
			if (ans[i]=='#')ans[i]='\t';
		}
		cout<<ans<<endl;
		return 0;
	}
	else {
		return action("List",sockfd);
	}
	return 1;
}

bool post_action(bool fail,string* options,int req_index){	// 0 for continue
	string req=options[req_index];

	if (fail){
		cout<<req<<" failed!"<<endl<<endl;
		display_option(options);
		return 0;
	}
	else {
		if (req=="Login"){
			options[1]="List";
			options[2]="Logout";
		}
    	else if (req=="Logout"){
    		cout<<"Logout : by user request."<<endl<<endl;
    		return 1;
    	}
    	display_option(options);
    	return 0;
	}
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

    	string req,reqstr;
    	string ans="test";
    	bool fail=0;

    	ans=recv_msg(c_socket);
    	cout<<ans<<endl;
    	//finish connection
    	
    	string options[10];
    	options[0]="2";
    	options[1]="Register";
    	options[2]="Login";
    	display_option(options);
    	while (getline(cin,req)){
    		if ((reqstr=make_req(req,options))=="error"){
    			cout<<endl;
    			display_option(options);
    			continue;
    		}
    		send_msg(c_socket,reqstr);
    		sleep(0.25);
    		cout<<"Output:"<<endl<<endl;
    		fail=action(reqstr,c_socket);

    		if (post_action(fail,options,atoi(req.c_str()))){
    			close(c_socket);
    			break;
    		}
    		
    	}

    	if (reqstr!="Exit"){
    		cout<<"Logout : by unknown reason."<<endl;
    	}

    	return 0;
	}
	else {
		cout<<"Could not find hostip"<<endl;
		return 1;
	}

}

int main(int argc, char const *argv[]){
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