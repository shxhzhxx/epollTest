#include "push.h"
#include <sys/ioctl.h>
#include <string.h>
#include <arpa/inet.h>


int main(int argc,char *argv[]){
	if(argc<2){
		printf("Need to specify a port to start the process, use\n\n    %s port\n\n", argv[0]);
		exit(-1);
	}

	daemonize("push_server");
	char *path=getcwd(NULL,0);
	if(path==NULL){
		exit(-1);
	}
	Log logger(path);
	delete path;

	uint32_t buff_size=0;
	if(argc>2){
		buff_size=atoi(argv[2]);
	}
	if(buff_size<=0){
		buff_size=10*1024;
	}
	uint32_t buff_size_big_endian = htonl(buff_size);


	std::unordered_set<int> unbound_clients;
	std::unordered_map<int, int> bound_clients;
	std::unordered_map<int, int>::iterator search;
	char buff[buff_size];
	struct KeepConfig cfg = { 5, 2, 2};

	uint32_t max_events=10;
	struct epoll_event ev, events[max_events];
	uint32_t servfd, sockfd, n, nfds, epollfd;
	uint32_t len,len_2,num;
	uint64_t id;
	struct sockaddr_in addr;
    socklen_t addrlen;
    char address[INET_ADDRSTRLEN];

	if((servfd=initTcpServer(argv[1]))==-1){
		logger.printf("initTcpServer failed\n");
		exit(-1);
	}

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		logger.printf("epoll_create1 failed\n");
		exit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64=servfd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, servfd, &ev) == -1) {
		logger.printf("epoll_ctl: servfd failed\n");
		exit(-1);
	}

	logger.printf("init success\n");

	auto closeClient = [&bound_clients,&unbound_clients](int id,int sockfd){
		if(id!=0){
			bound_clients.erase(id);
		}else{
			unbound_clients.erase(sockfd);
		}
		close(sockfd);
	};

	for (;;) {
	   nfds = epoll_wait(epollfd, events, max_events, -1);
	   if (nfds == -1) {
			logger.printf("epoll_wait failed\n");
	    	exit(-1);
	   }

	   for (n = 0; n < nfds; ++n) {
	   		sockfd=events[n].data.u64;
	       if (sockfd == servfd) {
	           sockfd = accept(servfd,NULL,NULL);
	           if (sockfd == -1) {
					logger.printf("accept failed\n");
	        		exit(-1);
	           }
	           set_tcp_keepalive_cfg(sockfd, &cfg);
	           ev.events = EPOLLIN | EPOLLET;
	           ev.data.u64 = sockfd;
	           if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd,&ev) == -1) {
					logger.printf("epoll_ctl: sockfd failed\n");
					exit(-1);
	           }else{
	           		unbound_clients.emplace(sockfd);
	           }
	       } else {
			   ioctl(sockfd,FIONREAD,&len);
			   logger.printf("len:%d\n",len);
			   len_2=recv(sockfd,buff,buff_size,MSG_DONTWAIT|MSG_PEEK);
			   logger.printf("len_2:%d\n",len_2);
			   send(sockfd,buff,len_2,MSG_NOSIGNAL);
			   continue;
	       }
	   }
	}
}