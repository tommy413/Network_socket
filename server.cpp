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
#include <malloc.h>
#include <resolv.h>
#include "openssl/ssl.h"
#include "openssl/err.h"

#define MaxThread 2 					//worker_pool內的Thread總數
#define BLEN 102400

using namespace std;

int maxfd=0;
int OnlineNum=0;						//線上人數
queue<int> socketQ;						//等待worker_pool處理的socket
queue<pthread_t> worker_pool;
pthread_mutex_t Qlock,IOlock,Datalock,Selectlock;
fd_set readfds,userfds;
map<string , int > accounts;			//以註冊的帳戶資訊
map<string , pair<string,int> > online;	//線上的帳戶資訊
map<int , string> sock2account;
map<int , sockaddr_in> sock2addr;
map<int , SSL*> sock2ssl;
SSL_CTX *ctx;


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

//格式化ip字串
string getip(sockaddr_in* ip){
	ostringstream sout;
	sout<<int(ip->sin_addr.s_addr&0xFF)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF00)>>8)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF0000)>>16)<<"."<<
  			int((ip->sin_addr.s_addr&0xFF000000)>>24);
	return sout.str();
}

//Init server instance and context
SSL_CTX* InitServerCTX(void)
{   
    const SSL_METHOD *method;
    SSL_CTX *fctx;
 
    OpenSSL_add_all_algorithms();  //load & register all cryptos, etc. 
    SSL_load_error_strings();   // load all error messages */
    method = TLSv1_server_method();  // create new server-method instance 
    fctx = SSL_CTX_new(method);   // create new context from method
    if ( fctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return fctx;
}

//Load the certificate 
void LoadCertificates(SSL_CTX* fctx, string CertFile, string KeyFile)
{
    //set the local certificate from CertFile
    if ( SSL_CTX_use_certificate_file(fctx, CertFile.c_str(), SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    //set the private key from KeyFile
    if ( SSL_CTX_use_PrivateKey_file(fctx, KeyFile.c_str(), SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    //verify private key 
    if ( !SSL_CTX_check_private_key(fctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        abort();
    }
}

bool send_msg(int sockfd, string msg , SSL* fssl){	//0 for success
	//if (send(sockfd,msg.c_str(),msg.size(),0)<0){
	if (SSL_write(fssl, msg.c_str(), msg.size()) <0 ){ // encrypt & send message 
		//cout<<msg<<endl;
		cout<<"Error: Sending msg."<<endl;
		return 1;
	}
	return 0;
}

string recv_msg(int sockfd , SSL* fssl){
	char buf[BLEN];
	char *bptr=buf;
	int buflen=BLEN;
	string ans="";
	int n;

	memset(buf, '\0', sizeof(buf)); 

	n = SSL_read(fssl, bptr, buflen);
	if ( n < 0 ){
		cout<<"Recieving Error!"<<endl;
		return "error";
	}
	if (n==0){
		close(sockfd);
    	FD_CLR(sockfd,&userfds);
    	return "Connection cut";
	}
	string tmp(buf);
	ans=tmp;
	return ans;
}

//處理client所送來的request(邏輯部分)
bool action(string req, int connectfd){
	SSL* ssl = sock2ssl[connectfd];
	int findpos,squpos;
	map<string , pair<string,int> >::iterator online_it;
	map<string , int >::iterator accounts_it;
	string ans="";
	if ((findpos=req.find("REGISTER"))>=0){	//register
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
			send_msg(connectfd,ans,ssl);
			return 0;
		}
		else {
			pthread_mutex_unlock(&Datalock);
			ans = "210 FAIL\n";
			send_msg(connectfd,ans,ssl);
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
		send_msg(connectfd,ans,ssl);
		return 0;
	}
	else if ((findpos=req.find("Exit"))>=0){	//Exit
		pthread_mutex_lock(&Datalock);
		online.erase(sock2account[connectfd]);
		sock2account.erase(connectfd);
		sock2addr.erase(connectfd);
		sock2ssl.erase(connectfd);
		OnlineNum--;
		pthread_mutex_unlock(&Datalock);
		send_msg(connectfd, "bye",ssl);
		SSL_free(ssl);
		close(connectfd);
		FD_CLR(connectfd,&userfds);
		return 0;
	}
	else if ((findpos=req.find("Trade"))>=0){	//Trade
		squpos=req.find("#");
		req = req.substr(squpos+1,req.size()-squpos);

		online_it = online.find(req);
		if (online_it!=online.end()){
			ans = online[req].first+"#"+itos(online[req].second);
			send_msg(connectfd,ans,ssl);
			return 0;
		}
		send_msg(connectfd,"The user is not online!!!",ssl);
		return 1;
	}
	else if ((findpos=req.find("Change"))>=0){	//Change
		squpos=req.find("#");
		req = req.substr(squpos+1,req.size()-squpos);
		squpos=req.find("#");
		string acc = req.substr(0,squpos);
		int money = stoi(req.substr(squpos+1,req.size()-squpos));

		accounts_it = accounts.find(acc);
		if (accounts_it != accounts.end()){
			accounts[acc]=accounts[acc]+money;
			ans="OK";
			send_msg(connectfd,ans,ssl);
			return 0;
		}
		send_msg(connectfd,"Changing Failed!!! Please Retransmit the request again!!!",ssl);
		return 1;
	}
	else {	//login
		squpos=req.find("#");
		if (squpos<0){
			ans = "200 Usage Error\n";
			send_msg(connectfd,ans,ssl);
			return 1;
		}
		string acc = req.substr(0,squpos);
		int port = stoi(req.substr(squpos+1,req.size()-squpos));
		if ((port > 65535) || (port < 2000)) {
			ans = "200 Usage Error\n";
			send_msg(connectfd,ans,ssl);
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
				send_msg(connectfd,ans,ssl);
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
			send_msg(connectfd,ans,ssl);
			return 0;
		}
		else {
			pthread_mutex_unlock(&Datalock);
			ans = "220 AUTH_FAIL\n";
			send_msg(connectfd,ans,ssl);
			return 1;
		}
	}
}

//worker所執行的函數
void* handle(void* DummyPT){
	int connectfd = -1;
	SSL* ssl;
	while (1){
		/*
		監看SocketQ
		若有工作，拿到socket編號並處理request
		若沒有則繼續等待
		 */
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

		/*
		如果之前有拿到Socket編號則處理request
		 */
		string entry="";
		if(connectfd >= 0) {
			pthread_mutex_lock(&Datalock);
			ssl = sock2ssl[connectfd];
			pthread_mutex_unlock(&Datalock);
			entry = recv_msg(connectfd,ssl);
			pthread_mutex_lock(&IOlock);
			//特別處理client Ctrl-C斷線的情況
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
				sock2ssl.erase(connectfd);
				pthread_mutex_unlock(&Datalock);
				SSL_free(ssl);
    	    }
    	    //正常request
    	    else {
    	    	pthread_mutex_unlock(&IOlock);
    	    	action(entry,connectfd);
    	    }
    	}
    	//表示已到一次select的結尾，可以進行下一次的select(配合manage函數)
    	else if(connectfd == -1){
    		pthread_mutex_unlock(&Selectlock);
    		sleep(1);
    	}
	}
    return NULL;
}

//創立worker_pool
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
	SSL* ssl = sock2ssl[s_socket];
	SSL* c_ssl;
	struct sockaddr_in clin_addr;
	socklen_t client_len= sizeof(clin_addr);
			
	while (1){
		//預防一個request因為worker處理較慢而被select很多次的狀況，因此用mutex來控制
		pthread_mutex_lock(&Selectlock);
		readfds=userfds;
		select(maxfd+1,&readfds,NULL,NULL,NULL);
		for (int i = 0; i <= maxfd; ++i)
		{
			if (FD_ISSET(i,&readfds)){
				//有新連線
				if (i==s_socket){
					connectfd = accept(s_socket, (struct sockaddr *) &clin_addr, &client_len);
					if (connectfd<0){
			    		pthread_mutex_lock(&IOlock);
			    		cout<<"Acception Failed!"<<endl;
			    		pthread_mutex_unlock(&IOlock);
 				   		continue;
    				}
    				c_ssl = SSL_new(ctx);
    				SSL_set_fd(c_ssl, connectfd);      // set connection socket to SSL state
    				if ( SSL_accept(c_ssl) < 0){ //do SSL-protocol accept
    					pthread_mutex_lock(&IOlock);
			    		cout<<"SSL Acception Failed!"<<endl;
			    		pthread_mutex_unlock(&IOlock);
			    		SSL_free(c_ssl);
			    		continue;
    				}
    				sock2ssl[connectfd]=c_ssl;
    				sock2addr[connectfd]=clin_addr;
    				send_msg(connectfd,"Connection Success!!\n",c_ssl);
    				FD_SET(connectfd,&userfds);
    				maxfd=max(maxfd,connectfd);
				}
				//client有新request
				else {
					pthread_mutex_lock(&Qlock);
					socketQ.push(i);
					pthread_mutex_unlock(&Qlock);
				}
			}
		}
		//以-1表示一輪select的結尾
		sleep(1.5);
		pthread_mutex_lock(&Qlock);
		socketQ.push(-1);
		pthread_mutex_unlock(&Qlock);
	}
	return NULL;
}

bool server_socket(int port){	//0 for success
	int s_socket,connectfd;
	struct sockaddr_in serv_addr,clin_addr;
	SSL* ssl;
	pthread_t manager,accepter;
	
	pthread_mutex_init(&Qlock,NULL);
	pthread_mutex_init(&IOlock,NULL);
	pthread_mutex_init(&Datalock,NULL);
	pthread_mutex_init(&Selectlock,NULL);
	FD_ZERO(&readfds);
	FD_ZERO(&userfds);
	SSL_library_init(); //init SSL library
	ctx = InitServerCTX();  //initialize SSL 
    LoadCertificates(ctx, "mycert.pem", "mykey.pem"); // load certs and key
    
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
    ssl = SSL_new(ctx);              // get new SSL state with context 
    SSL_set_fd(ssl, s_socket);      // set connection socket to SSL state
    sock2ssl[s_socket]=ssl;
    FD_SET(s_socket,&userfds);
    maxfd=max(s_socket,maxfd);
    pthread_create(&manager,NULL,&manage,&s_socket);	//負責select的thread
    
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