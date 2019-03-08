#ifndef __COMMON_H__
#define __COMMON_H__



#define BACKLOG 	10 
#define LENGTH 		256              		// Buffer length 

#define MAX_SEND_BUFFER_COUNT  10
#define MAX_SEND_BUFFER_CONTENT_SIZE  100

#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef int (*fun_data_recv_ptr)(char*, int, char*, int); // 声明一个指向同样参数、返回值的函数指针类型

typedef struct {
	char buffer[100];
	int len;
}QueueBuffer;

typedef struct {
	QueueBuffer buffer[MAX_SEND_BUFFER_COUNT];
	int pushindex;
	int popindex;
	pthread_spinlock_t lock;
}QueueManager;


typedef struct {
    char name[20];
    char dev[50];
    int speed;
	int fd;
	char buf[100];
	QueueManager sendqueue;
	int len;
    int opened;
    int exitflag;
	pthread_t handler;

    fun_data_recv_ptr callback;

}SerialPortInfo;

typedef struct {
    char name[20];
    char ip[50];
    int port;
	int fd;
	char buf[100];
	int len;
	QueueManager sendqueue;
    int connected;
    int exitflag;
	int isretesting;
	pthread_t handler;

    fun_data_recv_ptr callback;

}SocketInfo;

typedef struct {
	SerialPortInfo* RFID;
	SocketInfo* PLC;
	SocketInfo* PLC_Mac1;
	SocketInfo* PLC_Mac2;
	SocketInfo* PLC_Mac3;
	SocketInfo* MacMini1;
	SocketInfo* MacMini2;
	SocketInfo* MacMini3;
	SocketInfo* Test1;
	SocketInfo* Test2;
	SocketInfo* Test3;
}DeviceContainerInfo;



#endif

