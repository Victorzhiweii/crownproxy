#include <stdlib.h>
#include <stdio.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/wait.h> 
#include <sys/socket.h> 
#include <unistd.h> 
#include <pthread.h> 
#include <arpa/inet.h> 
#include <linux/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include "sock.h"
#include "uart.h" 
#include <dirent.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "common.h" 
#include <netdb.h>


void tcpSocketThread(void *arg)
{
	SocketInfo* info = (SocketInfo*)arg;
	
	int len;
	char revbuf[256];
	int fd;
    struct hostent *hostaddr;    /* structure that will get information about remote host */
	struct sockaddr_in addr;

	//printf("tcpThread(0001)\n");
	
    info->fd = -1;
    info->exitflag = 0;
    info->connected = 0;
    memset(info->buf, 0, sizeof(info->buf));
    info->len = 0;

	//printf("tcpThread(0002)\n");
    hostaddr = gethostbyname(info->ip);
    if(hostaddr == NULL)
    {
        printf("tcpThread(1-%s), gethostbyname error:%d for %s\n", info->name, errno, info->ip);
        return;
    }
	//printf("tcpThread(0003)\n");
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(info->port);
    addr.sin_addr = *((struct in_addr *)hostaddr->h_addr);

	//printf("tcpThread(0004)\n");
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if(fd == -1)
    {
        printf("tcpThread(2-%s), new socket error:%d - %s\n", info->name, errno, strerror( errno ));
        
        // usleep(1000*500); // 0.5s
        // continue;
        return;
    }
    info->fd = fd;
    printf("tcpThread(3-%s),  new socket success is %d\n", info->name, fd);

	while(info->exitflag == 0)
	{
        if(fd == -1)
        {
            fd = socket(AF_INET, SOCK_STREAM, 0);
            if(fd == -1)
            {
                printf("tcpThread(2-%s), new socket error:%d - %s\n", 
                        info->name, errno, strerror(errno ));
                
                usleep(1000*1500); // 0.5s
                continue;
            }
            info->fd = fd;
            //printf("tcpThread(3-%s),  new socket success is %d\n", info->name, fd);
        }

        if(info->connected == 0){

            if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
            {
                info->connected = 0;
                printf("tcpThread(4-%s), conn to %s:%d error : %d - %s\n", 
                                    info->name, info->ip, info->port, errno, strerror(errno));
                // printf("tcpThread(4-%s), connect port is %d\n", info->name, info->port);
                // printf("tcpThread(4-%s), connect ip is %s\n", info->name, info->ip);
                //printf("connect() error socked is %d\n", sockfd);

                usleep(1000*5000); // 5s
                continue;
            }

            info->connected = 1;
            
            printf("tcpThread(5-%s), conn success, ip is %s, port is %d\n", info->name, info->ip, info->port);

            // /* set NONBLOCK */
            // flags = fcntl(fd, F_GETFL, 0);
            // fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            {
                struct timeval timeout={0,500*1000};//3s
                int ret=setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
                printf("tcpThread(50-%s), set timeout result:%d\n", info->name, ret);
            }
        }
        
		len=recv(fd, revbuf, sizeof(revbuf), 0);//MSG_DONTWAIT);
		if(len == 0) 
		{ // 网络断开
			printf("tcpThread(6-%s), recv error, socket lost \n", info->name); 

            close(fd);
            fd = -1;
            info->connected = 0;

            usleep(1000*5000); // 5.0s
			continue;
		}

		if(len > 0)
		{
            // 清除尾部的无效字符串，只容许asscii为：32 ～ 126的字符串
            while(len > 0)
            {
                char d = revbuf[len-1];
                if(d == '\0' || d == '\r' || d == '\n' || d == ' ' || d < 32)
                {
                    len--;
                    printf("tcpThread(13-%s), remove invalid char pos : %d , value: 0x%02X \n", 
                        info->name, len, d);
                }
                else
                {
                    //printf("rfidSerialPortThread(12-%s), len : %d , value: 0x%02X \n", info->name, len, d);
                    break;
                }
            }

            if(info->callback != NULL && len > 0)
            {
                info->callback(info->name, fd, revbuf, len);
            }
            else
            {
                printf("tcpThread(13-%s), no recv any valid data\n", info->name);
            }
		}

		if(info->sendqueue.pushindex != info->sendqueue.popindex)
		{
            QueueManager* queuemgr = &(info->sendqueue);

			#if 1
			printf("tcpThread(20-%s), push: %d , pop: %d \n", 
                    info->name, queuemgr->pushindex, queuemgr->popindex);
			
			pthread_spin_lock(&(queuemgr->lock));

			QueueBuffer* ptr = queuemgr->buffer + queuemgr->popindex;

			queuemgr->popindex++;
			if(queuemgr->popindex >= MAX_SEND_BUFFER_COUNT)
			{
				queuemgr->popindex = 0;
			}
			pthread_spin_unlock(&(queuemgr->lock));

            if(ptr->len > 0)
            {
                len = write(info->fd, ptr->buffer, ptr->len);
                if(len != ptr->len)
                {
                    printf("tcpThread(21-%s), send to socket err: %d != %d, err: %d, push: %d , pop: %d \n", 
                            info->name, len, ptr->len, errno, queuemgr->pushindex, queuemgr->popindex);
                    //notifyresult(info, "ERROR_10");
                    if(info->callback != NULL)
                    {
                        info->callback(info->name, info->fd, "ERROR_10", strlen("ERROR_10"));
                    }
                }
                else
                {
                    printf("tcpThread(22-%s), send to socket success len : %d, push: %d , pop: %d \n", 
                        info->name, len, queuemgr->pushindex, queuemgr->popindex);

                    usleep(1000 * 100);
                }
            }
			else
            {
                printf("tcpThread(23-%s), no any data to send, push: %d , pop: %d \n", 
						info->name, queuemgr->pushindex, queuemgr->popindex);
            }
            
			#endif
		}
	}

    if(fd >= 0)
        close(fd);

    info->connected = 0;
    info->fd = -1;
}
