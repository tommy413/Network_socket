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
#include <queue>
#include <map>

#define MaxThread 2
#define BLEN 102400

using namespace std;

int maxfd=0;
int OnlineNum=0;
queue<int> socketQ;
queue<pthread_t> worker_pool;
pthread_mutex_t Qlock,IOlock,Datalock,Selectlock;
fd_set readfds,userfds;
map<string , int > accounts;
map<string , pair<string,int> > online;
map<int , string> sock2account;
map<int , sockaddr_in> sock2addr;

int stoi(string s){
	int i=-1;
	istringstream sin(s);
	sin>>i;
	return i;
}

string itos(int i){
	ostringstream sout;
	sout<<i;
	return sout.str();
}

string getip(sockaddr_in* ip){
	ostringstream sout;
	sout<<int(ip->sin_addr.s_addr&0xFF)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF00)>>8)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF0000)>>16)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF000000)>>24);
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

	int n = recv(sockfd, bptr, buflen, 0);
	if ( n < 0 ){
		cout<<"Recieving Error!"<<endl;
		return "error";
	}
	if (n==0){
		return "Connection cut";
	}
	string tmp(buf);
	ans=tmp;
	return ans;
}

bool action(string req, int connectfd){
	int findpos,squpos;
	map<string , pair<string,int> >::iterator online_it;
	map<string , int >::iterator accounts_it;
	string ans="";
	if ((findpos=req.find("REGISTER"))>=0){
		squpos=req.find("#");
		req = req.substr(squpos+1,req.size()-squpos);
		squpos=req.find("#");
		string acc = req.substr(0,squpos);
		int money = stoi(req.substr(squpos+1,req.size()-squpos));
		pthread_mutex_lock(&Datalock);
		accounts_it = accounts.find(acc);
		if (accounts_it==accounts.end()){
			accounts[acc] = money;
			pthread_mutex_unlock(&Datalock);	
			ans = "100 OK\n";
			send_msg(connectfd,ans);
			return 0;
		}
		else {
			pthread_mutex_unlock(&Datalock);
			ans = "210 FAIL\n";
			send_msg(connectfd,ans);
			return 1;
		}
	}
	else if ((findpos=req.find("List"))>=0){	//List
		pthread_mutex_lock(&Datalock);
		ans="accountBalance : " + itos(accounts[sock2account[connectfd]])+"\n";
		ans=ans+"NumberOfAccountsOnline : " + itos(OnlineNum) +"\n";
		for (online_it=online.begin(); online_it!=online.end(); ++online_it)
		{
			pair<string,int> info = online_it->second;
			ans=ans+online_it->first+"#"+info.first+"#"+itos(info.second)+"\n";
		}
		pthread_mutex_unlock(&Datalock);
		send_msg(connectfd,ans);
		return 0;
	}
	else if ((findpos=req.find("Exit"))>=0){	//Exit
		pthread_mutex_lock(&Datalock);
		online.erase(sock2account[connectfd]);
		sock2account.erase(connectfd);
		sock2addr.erase(connectfd);
		OnlineNum--;
		pthread_mutex_unlock(&Datalock);
		send_msg(connectfd, "bye");
		close(connectfd);
		FD_CLR(connectfd,&userfds);
		return 0;
	}
	else {	//login
		squpos=req.find("#");
		if (squpos<0){
			ans = "200 Usage Error\n";
			send_msg(connectfd,ans);
			return 1;
		}
		string acc = req.substr(0,squpos);
		int port = stoi(req.substr(squpos+1,req.size()-squpos));
		if ((port > 65535) || (port < 2000)) {
			ans = "200 Usage Error\n";
			send_msg(connectfd,ans);
			return 1;
		}
		pthread_mutex_lock(&Datalock);
		accounts_it = accounts.find(acc);
		if (accounts_it!=accounts.end()){
			sock2account[connectfd]=acc;
			online_it = online.find(acc);
			if (online_it!=online.end()){
				pthread_mutex_unlock(&Datalock);
				ans = "The account has already logined!\n";
				send_msg(connectfd,ans);
				return 1;
			}
			online[acc] = make_pair(getip(&sock2addr[connectfd]),port);//TODO:重複登入
			OnlineNum++;
			ans="accountBalance : " + itos(accounts[sock2account[connectfd]])+"\n";
			ans=ans+"NumberOfAccountsOnline : " + itos(OnlineNum) +"\n";
			for (online_it=online.begin(); online_it!=online.end(); ++online_it)
			{
				pair<string,int> info = online_it->second;
				ans=ans+online_it->first+"#"+info.first+"#"+itos(info.second)+"\n";
			}
			pthread_mutex_unlock(&Datalock);
			send_msg(connectfd,ans);
			return 0;
		}
		else {
			pthread_mutex_unlock(&Datalock);
			ans = "220 AUTH_FAIL\n";
			send_msg(connectfd,ans);
			return 1;
		}
	}
}

void* handle(void* DummyPT){
	int connectfd = -1;
	pthread_t threadid = pthread_self();
	while (1){
		connectfd = -1;
		pthread_mutex_lock(&Qlock);
		if (!socketQ.empty()){
			connectfd = socketQ.front();
			socketQ.pop();
			pthread_mutex_unlock(&Qlock);
		}
		else {
			pthread_mutex_unlock(&Qlock);
			continue;
		}

		string entry="";
		if(connectfd >= 0) {
			entry = recv_msg(connectfd);
			//cout<<entry<<endl;
			pthread_mutex_lock(&IOlock);
    	    if (entry=="Connection cut"){
    	    	cout << entry <<" : "<<itos(connectfd)<< endl;
    	    	pthread_mutex_unlock(&IOlock);
    	    	pthread_mutex_lock(&Datalock);
    	    	map<int , string>::iterator sock_it;
    	    	sock_it = sock2account.find(connectfd);
				if (sock_it!=sock2account.end()){
					online.erase(sock2account[connectfd]);
					sock2account.erase(connectfd);
					OnlineNum--;
				}
				sock2addr.erase(connectfd);
				pthread_mutex_unlock(&Datalock);
    	    	close(connectfd);
    	    	FD_CLR(connectfd,&userfds);
    	    }
    	    else {
    	    	pthread_mutex_unlock(&IOlock);
    	    	action(entry,connectfd);
    	    }
    	}
    	else if(connectfd == -1){
    		pthread_mutex_unlock(&Selectlock);
    	}
	}
    return NULL;
}

void makeThread(){
	pthread_t tmp;
	while (worker_pool.size()<MaxThread){
		pthread_create(&tmp,NULL,&handle,NULL);
		worker_pool.push(tmp);
	}
	return ; 
}

void* manage(void* SocketPT){
	int connectfd,s_socket=*((int*)SocketPT);
	struct sockaddr_in clin_addr;
	socklen_t client_len= sizeof(clin_addr);
		
	while (1){
		pthread_mutex_lock(&Selectlock);
		readfds=userfds;
		select(maxfd+1,&readfds,NULL,NULL,NULL);
		for (int i = 0; i <= maxfd; ++i)
		{
			if (FD_ISSET(i,&readfds)){
				if (i==s_socket){
					connectfd = accept(s_socket, (struct sockaddr *) &clin_addr, &client_len);
    				if (connectfd<0){
			    		pthread_mutex_lock(&IOlock);
			    		cout<<"Acception Failed!"<<endl;
			    		pthread_mutex_unlock(&IOlock);
 				   		continue;
    				}
    				sock2addr[connectfd]=clin_addr;
    				send_msg(connectfd,"Connection Success!!\n");
    				FD_SET(connectfd,&userfds);
    				maxfd=max(maxfd,connectfd);
				}
				else {
					pthread_mutex_lock(&Qlock);
					socketQ.push(i);
					pthread_mutex_unlock(&Qlock);
				}
			}
		}
		pthread_mutex_lock(&Qlock);
		socketQ.push(-1);
		pthread_mutex_unlock(&Qlock);
	}
	return NULL;
}

bool server_socket(int port){	//0 for success
	int s_socket,connectfd;
	struct sockaddr_in serv_addr,clin_addr;
	pthread_mutex_init(&Qlock,NULL);
	pthread_mutex_init(&IOlock,NULL);
	pthread_mutex_init(&Datalock,NULL);
	pthread_mutex_init(&Selectlock,NULL);
	FD_ZERO(&readfds);
	FD_ZERO(&userfds);
	pthread_t manager,accepter;

	while (!socketQ.empty())socketQ.pop();

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

    if (listen(s_socket,10) != 0){
    	cout<<"Listening Error!!"<<endl;
    	return 1;
    }

    makeThread();
    FD_SET(s_socket,&userfds);
    maxfd=max(s_socket,maxfd);
    pthread_create(&manager,NULL,&manage,&s_socket);
    
    string cmd;
    map<string , pair<string,int> >::iterator online_it;
	map<string , int >::iterator accounts_it;
	map<int, string>::iterator sock2account_it;
    while(cin>>cmd){
    	if (cmd=="Close")break;
    	if (cmd=="List_Account"){
    		for (accounts_it = accounts.begin(); accounts_it != accounts.end(); ++accounts_it)
    		{
    			cout<<accounts_it->first<<" "<<accounts_it->second<<endl;
    		}
    	}
    	if (cmd=="List_Online"){
    		for (online_it = online.begin(); online_it != online.end(); ++online_it)
    		{
    			cout<<online_it->first<<" "<<online_it->second.first<<" "<<online_it->second.second<<endl;
    		}
    	}
    	if (cmd=="List_Socket"){
    		for (sock2account_it = sock2account.begin(); sock2account_it != sock2account.end(); ++sock2account_it)
    		{
    			cout<<sock2account_it->first<<" "<<sock2account_it->second<<endl;
    		}
    	}
    }

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