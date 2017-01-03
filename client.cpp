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
#include <pthread.h>
#include <malloc.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"

#define BLEN 102400

using namespace std;
string LoginName="";
pthread_t P2Pserver;
string options[10];
bool action(string req,int sockfd);

//字串轉整數
int stoi(string s){
	int i=-1;
	istringstream sin(s);
	sin>>i;
	return i;
}

//整數轉字串
string itos(int i){
	ostringstream sout;
	sout<<i;
	return sout.str();
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

bool display_option(){ //0 for success
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
		
		cout<<"Money?"<<endl;
		getline(cin,args[1]);
		
		return "REGISTER#"+args[0]+"#"+args[1];
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
	else if (act=="Trade"){	
		cout<<"Trade User?"<<endl;
		getline(cin,args[0]);
		
		return act+"#"+args[0];
	}
	else if (act=="Logout"){
		return "Exit";
	}
}

void* P2P(void* InfoPT){
	int s_socket,connectfd;
	int maxfd=0;
	pair<int,int> tmp = *((pair<int,int>*)InfoPT);
	int port = tmp.first;
	int sockfd = tmp.second;
	struct sockaddr_in serv_addr,clin_addr;
	socklen_t client_len= sizeof(clin_addr);
	fd_set userfds,readfds;
	FD_ZERO(&readfds);
	FD_ZERO(&userfds);

	if ((s_socket = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
		cout<<"Could not create socket"<<endl;
		return NULL;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    
    if (bind(s_socket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
    	cout<<"Binding Error!!"<<endl;
    	return NULL;
    }

    if (listen(s_socket,10) != 0){
    	cout<<"Listening Error!!"<<endl;
    	return NULL;
    }

    FD_SET(s_socket,&userfds);
    maxfd=max(s_socket,maxfd);

    while(1){
    	readfds=userfds;
    	select(maxfd+1,&readfds,NULL,NULL,NULL);
		for (int i = 0; i <= maxfd; ++i)
		{
			if (FD_ISSET(i,&readfds)){
				if (i==s_socket){
					connectfd = accept(s_socket, (struct sockaddr *) &clin_addr, &client_len);
    				if (connectfd<0){
			    		cout<<"Acception Failed!"<<endl;
			    		continue;
    				}
    				FD_SET(connectfd,&userfds);
    				maxfd=max(maxfd,connectfd);
				}
				else {
					string req="",res;
					req = recv_msg(i);
					string acc="";
					string money;
					int findpos;

					//cout<<req<<endl;				//test
					if ((findpos=req.find("#")) >= 0){
						acc = req.substr(0,findpos);
						money = req.substr(findpos+1,req.size()-findpos);
					}
					else {
						send_msg(i,"Usage Error!!!");
					}
					cout<<endl<<"Here comes a trade request..."<<endl;
					cout<<"Trade from "<<acc<<" : "<<money<<endl;
					req = "Change#"+LoginName+"#"+money;
					send_msg(sockfd,req);
					string ans = recv_msg(sockfd);
					if (ans=="OK"){
    					send_msg(i,"OK");
    					send_msg(sockfd,"List");
    					close(i);
    					FD_CLR(i,&userfds);
    					action("List",sockfd);
    				}
    				else {
    					send_msg(i,"Trade Failed!!!");
    					close(i);
    					FD_CLR(i,&userfds);
    				}
    				display_option();
					sleep(1);
				}
			}
		}
    }
}

bool action(string req,int sockfd){	//0 for success
	string cmd="";
	string ans;
	int findpos;

	cout<<endl;
	if ((findpos=req.find("#")) >= 0){
		cmd = req.substr(0,findpos);
		req = req.substr(findpos+1,req.size()-findpos);
	}
	else {
		cmd = req;	
	}

	if (cmd=="Exit"){
		cout<<"Output:"<<endl<<endl;
		ans=recv_msg(sockfd);
		cout<<ans<<endl;
		if (ans!="bye") return 1;
		LoginName="";
		return 0;
	}
	else if (cmd=="REGISTER"){
		cout<<"Output:"<<endl<<endl;
		ans=recv_msg(sockfd);
		cout<<ans<<endl;
		if (ans!="100 OK\n") return 1;
		return 0;
	}
	else if (cmd=="List"){
		cout<<"Output:"<<endl<<endl;
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
	else if(cmd=="Trade"){
		ans=recv_msg(sockfd);
		string ipstr,portstr;
		if ((findpos=ans.find("#")) >= 0){
			ipstr = ans.substr(0,findpos);
			portstr = ans.substr(findpos+1,ans.size()-findpos);
		}
		else {
			cout<<ans<<endl;
			return 1;
		}

		string money;
		cout<<"Trade Amount?"<<endl;
		getline(cin,money);

		if (atoi(money.c_str()) < 0){
			cout<<"Please enter a positive number!!!"<<endl;
			return 1;
		}

		string tradereq="";
		tradereq = LoginName+"#"+money;

		struct hostent* hptr;
		struct sockaddr_in serv_addr;
		int trade_socket;
		
		if ((hptr = gethostbyname(ipstr.c_str())) < 0){
			cout<<"Cannot Find User's hostip!!!"<<endl;
			return 1;
		}

		if ((trade_socket = socket(AF_INET,SOCK_STREAM,0)) < 0){
			cout<<"Could not create socket"<<endl;
			return 1;
		}

		memset(&serv_addr, '0', sizeof(serv_addr)); 
    	serv_addr.sin_family = AF_INET;
    	memmove((char *)&serv_addr.sin_addr.s_addr,(char *)hptr->h_addr,hptr->h_length);
    	serv_addr.sin_port = htons(stoi(portstr));

    	if (connect(trade_socket,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
    		cout<<"Could not connect to host"<<endl;
			return 1;
    	}

    	send_msg(trade_socket,tradereq);
    	sleep(0.25);
    	ans = recv_msg(trade_socket);
    	if (ans=="OK"){
    		cout<<"Trade success"<<endl;
    		tradereq = "Change#"+LoginName+"#"+itos(-1*stoi(money));
    		send_msg(sockfd,tradereq);
    		ans = recv_msg(sockfd);
    		if (ans=="OK"){
    			send_msg(sockfd,"List");
    			close(trade_socket);
    			return action("List",sockfd);
    		}
    		else {
    			close(trade_socket);
    			return 1;
    		} 
    	}
    	else {
    		close(trade_socket);
    		return 1;
    	}
	}
	else {
		LoginName=cmd;
		pair<int , int> tmp;
		tmp = make_pair(stoi(req) , sockfd);
		pthread_create(&P2Pserver,NULL,&P2P,&tmp);
		return action("List",sockfd);
	}
	return 1;
}

bool post_action(bool fail,int req_index){	// 0 for continue
	string req=options[req_index];

	if (fail){
		cout<<req<<" failed!"<<endl<<endl;
		display_option();
		return 0;
	}
	else {
		if (req=="Login"){
			options[0]="3";
			options[1]="List";
			options[2]="Trade";
			options[3]="Logout";
		}
    	else if (req=="Logout"){
    		cout<<"Logout : by user request."<<endl<<endl;
    		return 1;
    	}
    	display_option();
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
   	
    	options[0]="2";
    	options[1]="Register";
    	options[2]="Login";
    	display_option();
    	while (1){
    		getline(cin,req);
			if ((reqstr=make_req(req,options))=="error"){
    			cout<<endl;
    			display_option();
    			continue;
    		}
    		send_msg(c_socket,reqstr);
    		sleep(0.25);
    		fail=action(reqstr,c_socket);

    		if (post_action(fail,atoi(req.c_str()))){
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