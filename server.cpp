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

#define MaxThread 2
#define BLEN 102400

using namespace std;

int maxfd=0;
queue<int> socketQ;
queue<pthread_t> worker_pool;
pthread_mutex_t Qlock,IOlock;
fd_set readfds,users;

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
		return "error";
	}
	string tmp(buf);
	ans=tmp;
	return ans;
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
			pthread_mutex_lock(&IOlock);
    	    cout << entry << endl;
    	    if(entry == "Exit"){
    	        send_msg(connectfd, "Bye");
    	        cout << "Enter response: Bye\n";
    	        pthread_mutex_unlock(&IOlock);
    	        FD_CLR(connectfd,&users);
    	        close(connectfd);
        	}
        	else {
        		cout << " response: ";
        		cout << entry <<endl;;
        		pthread_mutex_unlock(&IOlock);
        		send_msg(connectfd, entry);
        	}
        	
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
		readfds=users;
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
    				send_msg(connectfd,"Connection Success!!\n");
    				pthread_mutex_lock(&IOlock);
    				cout<<connectfd<<endl;
    				printip(&clin_addr);
    				cout<<endl;
    				pthread_mutex_unlock(&IOlock);
    				FD_SET(connectfd,&users);
    				maxfd=max(maxfd,connectfd);
				}
				else {
					pthread_mutex_lock(&Qlock);
					socketQ.push(i);
					cout<<"Pushing:"<<i<<endl;
					pthread_mutex_unlock(&Qlock);
					sleep(1.5);
				}
			}
		}

	}
	return NULL;
}

bool server_socket(int port){	//0 for success
	int s_socket,connectfd;
	struct sockaddr_in serv_addr,clin_addr;
	pthread_mutex_init(&Qlock,NULL);
	pthread_mutex_init(&IOlock,NULL);
	FD_ZERO(&readfds);
	FD_ZERO(&users);
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
    FD_SET(s_socket,&users);
    maxfd=max(s_socket,maxfd);
    pthread_create(&manager,NULL,&manage,&s_socket);
    
    /*string cmd;
    while(cin>>cmd){
    	if (cmd=="Close")break;
    }*/

    pthread_join(manager,NULL);
    while(!socketQ.empty()){}

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