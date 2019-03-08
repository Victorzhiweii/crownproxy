#include     <stdio.h> 
#include     <stdlib.h>
#include     <string.h>
#include     <unistd.h>
#include     <sys/types.h>
#include     <sys/stat.h>  
#include     <fcntl.h>  
#include     <termios.h>
#include     <errno.h> 
#include     <pthread.h> 
#include     <sys/ioctl.h> 
#include     "uart.h"
// #include 	 "tcpserver.h"
#include 	 "cJSON.h"
#include "common.h" 

void bellthread(void)
{
	am335x_gpio_arg arg;
	int fd = -1;

	fd = open("/dev/am335x_gpio", O_RDWR);

	arg.pin = GPIO_TO_PIN(1, 18);
	
	arg.data = 1;
	ioctl(fd, IOCTL_GPIO_SETOUTPUT, &arg);
	usleep(500*1000);
	arg.data = 0;
	ioctl(fd, IOCTL_GPIO_SETOUTPUT, &arg);
	close(fd);	
	//pthread_exit(0);
}

void set_speed(int fd, int speed)
{
	int speed_arr[] = {B115200, B57600, B38400, B19200, B9600, B4800,B2400, B1200};
	int name_arr[] = {115200, 57600, 38400,  19200,  9600,  4800,  2400, 1200};
	
	int   status;
	int i;
    struct termios   Opt;
    tcgetattr(fd, &Opt);
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
    {
   	if (speed == name_arr[i])
   	{
   	    tcflush(fd, TCIOFLUSH);
	    cfsetispeed(&Opt, speed_arr[i]);
	    cfsetospeed(&Opt, speed_arr[i]);
	    status = tcsetattr(fd, TCSANOW, &Opt);
	    if (status != 0)
		perror("tcsetattr fd1");
	    return;
     	}
	tcflush(fd,TCIOFLUSH);
    }
}

int set_Parity(int fd,int databits,int stopbits,int parity)
{
    struct termios options;
    if  ( tcgetattr( fd,&options)  !=  0)
    {
  	perror("SetupSerial 1");
  	return(FALSE);
    }
    options.c_cflag &= ~CSIZE;
    switch (databits)
    {
    case 7:
	options.c_cflag |= CS7;
	break;
    case 8:
	options.c_cflag |= CS8;
	break;
    default:
	fprintf(stderr,"Unsupported data size\n");
	return (FALSE);
    }
    switch (parity)
    {
    case 'n':
    case 'N':
	options.c_cflag &= ~PARENB;   
	options.c_iflag &= ~INPCK;   
	break;
    case 'o':
    case 'O':
	options.c_cflag |= (PARODD | PARENB); 
	options.c_iflag |= INPCK;           
	break;
    case 'e':
    case 'E':
	options.c_cflag |= PARENB;     
	options.c_cflag &= ~PARODD;
	options.c_iflag |= INPCK;     
	break;
    case 'S':
    case 's':  
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	break;
    default:
	fprintf(stderr,"Unsupported parity\n");
	return (FALSE);
    }
    switch (stopbits)
    {
    case 1:
	options.c_cflag &= ~CSTOPB;
	break;
    case 2:
	options.c_cflag |= CSTOPB;
	break;
    default:
	fprintf(stderr,"Unsupported stop bits\n");
	return (FALSE);
    }


	options.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);

    /* Set input parity option */
    
    if (parity != 'n')
    {
		options.c_iflag |= INPCK;
	}
	
    options.c_cc[VTIME] = 3; // 0.3 seconds
    options.c_cc[VMIN] = 0;
    
	
	tcflush(fd,TCIFLUSH); /* Update the options and do it NOW */
    if (tcsetattr(fd,TCSANOW,&options) != 0)
    {
		perror("SetupSerial 3");
		return (FALSE);
    }
    return (TRUE);
}



int uart_init(char *p, int speed)
{		
	int fd;
	fd = open(p, O_RDWR|O_NOCTTY);
	set_speed(fd,speed); //设置串口波特率	
	set_Parity(fd,8,1,'N'); //设置8位数据位，1位停止位，无校验等其他设置。
	
	return fd;	
}   	 
	    	
      
// void uart1thread(void) 	
// {
// 	uartmode *uart = (uartmode *)malloc(sizeof(uartmode)*5);
// 	int i,z;
// 	char sdbuf[LENGTH];
// 	int flag1 = 0;
// 	int sum = 0;
	
// 	FILE *f;
// 	long len;
// 	char *content;
// 	cJSON *json;
// 	cJSON *pSub;
// 	cJSON *format;
// 	cJSON *parameter;
// 	char *key;
// 	char *key1;
// 	int parameter_size;
// 	int format_size;
// 	pthread_t bell_1;
	
// 	uart[1].name = "/dev/ttyO1";
// 	uart[3].name = "/dev/ttyO3";

// 	f=fopen("./test.json","rb");
// 	fseek(f,0,SEEK_END);
// 	len=ftell(f);
// 	fseek(f,0,SEEK_SET);
// 	content=(char*)malloc(len+1);
// 	fread(content,1,len,f);
// 	fclose(f);

// 	 json=cJSON_Parse(content);

// 	parameter_size = cJSON_GetArraySize(json);

// 	for (i = 0; i < parameter_size; i++)
// 	{

// 		pSub = cJSON_GetArrayItem(json,i);	
// 		key = pSub->string;
// 		format = NULL;
// 		if(strcmp(key,"/dev/ttyO1") == 0)
// 		{	

// 			format = cJSON_GetObjectItem(json,"/dev/ttyO1");
// 			format_size = cJSON_GetArraySize(format);
// 			for(z = 0; z < format_size; z++)
// 			{
// 				parameter = cJSON_GetArrayItem(format,z);	
// 				key1 = parameter->string;
// 				if(strcmp(key1,"speed") == 0)
// 				{

// 					uart[1].speed = cJSON_GetObjectItem(format,"speed")->valueint;
				
// 				}				
// 			}
// 		}

// 		if(strcmp(key,"/dev/ttyO3") == 0)
// 		{	

// 			format = cJSON_GetObjectItem(json,"/dev/ttyO3");
// 			format_size = cJSON_GetArraySize(format);
// 			for(z = 0; z < format_size; z++)
// 			{
// 				parameter = cJSON_GetArrayItem(format,z);	
// 				key1 = parameter->string;
// 				if(strcmp(key1,"speed") == 0)
// 				{
// 					uart[3].speed = cJSON_GetObjectItem(format,"speed")->valueint;				
// 				}				
// 			}
// 		}
// 	}
// 	cJSON_Delete(json);

// 	uart[1].fd = uart_init(uart[1].name, uart[1].speed);
// 	uart[3].fd = uart_init(uart[3].name, uart[3].speed);

// 	char send_rfid1[] = {0xFF, 0x0D, 0xA0, 0x00, 0x00, 0x03, 0x10, 0x00, 0x01, 0xF4, 0x00, 0x00, 0x00, 0x23, 0x53, 0x20}; //模式设为自动寻卡模式 
// 	char send_rfid2[] = {0xFF, 0x04, 0x16, 0x00, 0x00, 0xE0, 0xD0}; //保存模式设置

// 	write(uart[1].fd, send_rfid1, sizeof(send_rfid1));			//模式设置为自动寻卡模式
	
// 	if((uart[1].len = read(uart[1].fd,uart[1].buf,200))>0) 		//排除未收到数据的可能
// 	{
// 		printf ("OK: RFID mode numbytes = %d\n", uart[1].len); 
// 		for(i = 0; i < uart[1].len; i++)
// 		{
// 			printf("%02x ", uart[1].buf[i]);
// 		}
// 		printf ("\nOK: RFID mode string \n");
// 	}

// 	write(uart[1].fd, send_rfid2, sizeof(send_rfid2));			//保存模式设置
// 	if((uart[1].len = read(uart[1].fd,uart[1].buf,200))>0)		//排除未收到数据的可能
// 	{
// 		printf ("OK: RFID mode numbytes = %d\n", uart[1].len); 
// 		for(i = 0; i < uart[1].len; i++)
// 		{
// 			printf("%02x ", uart[1].buf[i]);
// 		}
// 		printf ("\nOK: RFID mode string \n");
// 	}
	
// 	memset(sdbuf, 0, 200);

// 	while(1)    
//   	{
// 		if((sum = read(uart[1].fd,uart[1].buf,100))>0) //接收数据
// 		{

// 			printf ("OK: RFID numbytes = %d\n", sum); 		
// 			for(i = 0; i < sum; i++)
// 			{
// 				printf("%02x ", uart[1].buf[i]);
// 			}
// 			printf ("\nOK: RFID mode string \n");
			
// 		    pthread_create(&bell_1, NULL, (void*)bellthread,NULL);	
// 			for(i = 0; i < sum; i++)
// 			{
// 				sdbuf[i] = uart[1].buf[i + 7];
// 				if(uart[1].buf[i + 7] == 0)
// 				{
// 					flag1 = i;
// 					break;
// 				}				
// 			}
			
// 			write(uart[3].fd, sdbuf, flag1);
// 			write(uart[3].fd, "\n", sizeof("\n"));			
// 		}
// 		usleep(100/**1000*/);
//    } 
// 	printf("pthread_exit is open\n");
// 	pthread_exit(0); 	
// }




// void uart2thread(void * arg)
// {
// 	int socked;
// 	socked = *(int *)arg;
	
// 	uartmode *uart = (uartmode *)malloc(sizeof(uartmode)*3);
// 	int i,z;
// 	char sdbuf[LENGTH];
// 	int flag1 = 0;
// 	int sum = 0;
// 	pthread_t bell_1;
// 	FILE *f;
// 	long len;
// 	char *content;
// 	cJSON *json;
// 	cJSON *pSub;
// 	cJSON *format;
// 	cJSON *parameter;
// 	char *key;
// 	char *key1;
// 	int parameter_size;
// 	int format_size;
	
// 	uart[1].name = "/dev/ttyO1";

// 	f=fopen("./test.json","rb");
// 	fseek(f,0,SEEK_END);
// 	len=ftell(f);
// 	fseek(f,0,SEEK_SET);
// 	content=(char*)malloc(len+1);
// 	fread(content,1,len,f);
// 	fclose(f);

// 	 json=cJSON_Parse(content);

// 	parameter_size = cJSON_GetArraySize(json);

// 	for (i = 0; i < parameter_size; i++)
// 	{

// 		pSub = cJSON_GetArrayItem(json,i);	
// 		key = pSub->string;
// 		format = NULL;
// 		if(strcmp(key,"/dev/ttyO1") == 0)
// 		{	

// 			format = cJSON_GetObjectItem(json,"/dev/ttyO1");
// 			format_size = cJSON_GetArraySize(format);
// 			for(z = 0; z < format_size; z++)
// 			{
// 				parameter = cJSON_GetArrayItem(format,z);	
// 				key1 = parameter->string;
// 				if(strcmp(key1,"speed") == 0)
// 				{

// 					uart[1].speed = cJSON_GetObjectItem(format,"speed")->valueint;
				
// 				}				
// 			}
// 		}
// 	}
// 	cJSON_Delete(json);

// 	uart[1].fd = uart_init(uart[1].name, uart[1].speed);
	
// 	char send_rfid1[] = {0xFF, 0x0D, 0xA0, 0x00, 0x00, 0x03, 0x10, 0x00, 0x01, 0xF4, 0x00, 0x00, 0x00, 0x23, 0x53, 0x20}; //模式设为自动寻卡模式 
// 	char send_rfid2[] = {0xFF, 0x04, 0x16, 0x00, 0x00, 0xE0, 0xD0}; //保存模式设置

// 	write(uart[1].fd, send_rfid1, sizeof(send_rfid1));			//模式设置为自动寻卡模式
	
// 	if((uart[1].len = read(uart[1].fd,uart[1].buf,200))>0) 		//排除未收到数据的可能
// 	{
// 		printf ("OK: RFID mode numbytes = %d\n", uart[1].len); 
// 		for(i = 0; i < uart[1].len; i++)
// 		{
// 			printf("%02x ", uart[1].buf[i]);
// 		}
// 		printf ("\nOK: RFID mode string \n");
// 	}

// 	write(uart[1].fd, send_rfid2, sizeof(send_rfid2));			//保存模式设置
// 	if((uart[1].len = read(uart[1].fd,uart[1].buf,200))>0)		//排除未收到数据的可能
// 	{
// 		printf ("OK: RFID mode numbytes = %d\n", uart[1].len); 
// 		for(i = 0; i < uart[1].len; i++)
// 		{
// 			printf("%02x ", uart[1].buf[i]);
// 		}
// 		printf ("\nOK: RFID mode string \n");
// 	}

// 	memset(sdbuf, 0, 200);
	
// 	while(1)    
//   	{
// 		if((sum = read(uart[1].fd,uart[1].buf,100))>0) //接收数据
// 		{
// 		    pthread_create(&bell_1, NULL, (void*)bellthread,NULL);	
// 			for(i = 0; i < sum; i++)
// 			{
// 				sdbuf[i] = uart[1].buf[i + 7];
// 				if(uart[1].buf[i + 7] == 0)
// 				{
// 					flag1 = i;
// 					break;
// 				}				
// 			}
// 			send(socked,sdbuf,flag1,0);		
// 		}
// 		usleep(100/**1000*/);
//    } 
// 	printf("pthread_exit is open\n");
// 	pthread_exit(0); 	
// }


//static char send_rfid_cmd_auto_mode[] = {0xFF, 0x0D, 0xA0, 0x00, 0x00, 0x03, 0x10, 0x00, 0x01, 0xF4, 0x00, 0x00, 0x00, 0x23, 0x53, 0x20}; //模式设为自动寻卡模式 


static int init_rfid(int fd)
{
	int i, crc;
	char revbuf[LENGTH];
	int len = write(fd, send_rfid_cmd_manual_mode, sizeof(send_rfid_cmd_manual_mode));			//模式设为交互模式
	printf("init_rfid(1), set mode result:%d \n", len);

	usleep(1000 * 300);	
	len = read(fd, revbuf, sizeof(revbuf));
	
	printf("init_rfid(4), read change mode cmd response data size:%d \n", len);
	
	if(len > 0) 		//排除未收到数据的可能
 	{
		crc = crc_func(revbuf, len);
		if(crc != 0)											//crc校验，确保数据完整								
		{
			printf ("init_rfid(7) RFID response crc failed numbytes = %d\n", len); 
			for(i = 0; i < len; i++)
			{
				printf("%02x ", revbuf[i]);
			}
			printf ("\n");
			return crc;
		}
 	}
	else
	{
		return 1;
	}

 	len = write(fd, send_rfid_cmd_store, sizeof(send_rfid_cmd_store));			//保存模式设置
	printf("init_rfid(6), write data size:%d \n", len);
	usleep(1000 * 200);	
	len = read(fd, revbuf, sizeof(revbuf));
	
	printf("init_rfid(7), read store cmd response data size:%d \n", len);
	if(len > 0) 		//排除未收到数据的可能
	{
		crc = crc_func(revbuf, len);
		if(crc != 0)											//crc校验，确保数据完整								
		{
			printf ("init_rfid(7) RFID response failed numbytes = %d\n", len); 
			for(i = 0; i < len; i++)
			{
				printf("%02x ", revbuf[i]);
			}
			printf ("\n");

			return crc;
		}
	}
	else
	{
		return 1;
	}
	
	return 0;
}

void dumpbufferinfo(char* buffer, int len)
{
	int i;
	for(i = 0; i < len; i++)
	{
		if(buffer[i] >= 32 && buffer[i] < 127)
		{
			printf("%c ", buffer[i]);
		}
		else if(buffer[i] == '\0')
		{
			break;
		}
		else
		{
			printf("0x%02X ", buffer[i]);
		}
	}

	printf ("\n");
}

void notifyresult(SerialPortInfo* info, char* data)
{
	if(info->callback != NULL)
	{
		info->callback(info->name, info->fd, data, strlen(data));
	}
}

void rfidSerialPortThread(void * arg)
{
	SerialPortInfo* info = (SerialPortInfo*)arg;
	
	int len, i, crc, read_offset = 0;
	char revbuf[300];
  
	
	printf("rfidThread(0000)\n");
	info->exitflag = 0;
	info->opened = 0;
	memset(info->buf, 0, sizeof(info->buf));
    info->len = 0;

	printf("rfidThread(0-%s), entry uart thread, %s, %d\n", info->name, info->dev, info->speed);

	info->fd = uart_init(info->dev, info->speed);
	if(info->fd == -1){
		printf("rfidThread(1-%s), init uart:%d for %s\n", info->name, errno, info->dev);
        return;
	}

	printf("rfidThread(2-%s), init uart:%d success\n", info->name, info->fd);

	//fcntl(info->fd, F_SETFL, FNDELAY); //非阻塞
		
	
	len = init_rfid(info->fd);
	if(len != 0)
	{
		printf("rfidThread(3-%s), init rfid failed : %d\n", info->name, len);
	}

	//fcntl(info->fd, F_SETFL, 0); //阻塞

	while(info->exitflag == 0)
  	{
  		//printf("rfidThread(10-%s), read data size :%d \n", info->name, len);

		// struct  timeval start;
        // struct  timeval end;
        // unsigned  long diff;
        // gettimeofday(&start,NULL);
		
		len = read_offset + read(info->fd, revbuf + read_offset, sizeof(revbuf) - read_offset);

		// gettimeofday(&end,NULL);
		
		// diff = 1000 * (end.tv_sec-start.tv_sec)+ (end.tv_usec-start.tv_usec) / 1000;
		
        // printf("thedifference is %ld\n", diff);
		
		//printf("rfidThread(12-%s), read data size :%d \n", info->name, len);
		
		if(len > 0) //接收数据
		{
			if(revbuf[0] != 0xFF) 
			{
				printf("rfidThread(10-%s), read invalid header :%d , content: \n", info->name, len);
				#if 1 
				dumpbufferinfo(revbuf, len);
				#endif
			}
			else
			{
				if(read_offset == len)
				{

				}
				else if(len < 3 || ((unsigned char)revbuf[1]) > ((unsigned char)(len - 3)))
				{ // 数据没有读取完成
					
					printf("rfidThread(12-%s), not read completed : %d < totlen:%d, read_offset:%d  \n", 
							info->name, len, revbuf[1] + 3, read_offset);
					
					read_offset = len;

					usleep(1000 * 100);
					continue;
				}
			}
			
			read_offset = 0;

			// rfid包头不对
			if(revbuf[0] != 0xFF) 
			{
				printf("rfidThread(13-%s), read invalid header data :%d  \n", info->name, len);
				#if 1
					printf ("rfidThread(13-%s), read data numbytes = %d, content: \n", 
							info->name, len); 
					dumpbufferinfo(revbuf, len);
				#endif
				// 包头不正确
				strcpy(revbuf, "ERROR_09");
				len = strlen(revbuf);
			}
			else
			{
				crc = crc_func(revbuf, len);
				if(crc != 0)
				{
					printf("rfidThread(14-%s), rfid data crc :%d error, read data numbytes:%d, content: \n", 
							info->name, crc, len);
					#if 1
						dumpbufferinfo(revbuf, len);
					#endif

					// 数据校验失败
					strcpy(revbuf, "ERROR_07");
					len = strlen(revbuf);
				}
				else if(revbuf[5] != 0)
				{
					printf("rfidThread(15-%s), read data faild numbytes :%d, status:0x%x, content:\n", 
							info->name, len, revbuf[5]);
				#if 1
					dumpbufferinfo(revbuf, len);
				#endif
					// 返回错误状态码
					strcpy(revbuf, "ERROR_06");
					len = strlen(revbuf);
				}
			}
			//printf("rfidThread(11-%s), read data crc :%d , datalen:%d\n", info->name, crc, len);

			if(info->callback != NULL)
			{
				char* ptr = revbuf;
				if(len > 10)
				{
					#if 1
					dumpbufferinfo(revbuf,  len);
					#endif

					ptr = revbuf+ 6;
					len -= 8;
					// 清除尾部的空格
					while(len > 0)
					{
						char d = ptr[len-1];
						if(d == '\0' || d == '\r' || d == '\n' || d == ' ')
						{
							len--;
							printf("rfidThread(17-%s), remove invalid char pos : %d , value: 0x%02X \n", 
                        		info->name, len, d);
						}
						else
						{
							//printf("rfidThread(12-%s), len : %d , value: 0x%02X \n", info->name, len, d);
							break;
						}
					}
					// 判断是否有非法字符
					for(i=0; i < len; i++) 
					{
						unsigned char d = ptr[i];
						if(d >= 32 && d <= 126) 
						{
							continue;
						}
						else
						{
							printf("rfidThread(18-%s), index:%d found invalid char : 0x%02X \n", info->name, len, d);
							len = i;
							break;
						}
					}
					
					//info->callback(info->name, info->fd, ptr, len);
				}
				
				if(len > 0)
				{
					info->callback(info->name, info->fd, ptr, len);
				}
				else
				{
					printf("rfidThread(19-%s), no any valid data for read from rfid. \n", info->name);
				}
			}
		}

		read_offset = 0;

		// {
		// 	QueueManager* queuemgr = &(info->sendqueue);
		// 	pthread_spin_lock(&(queuemgr->lock));
		// 	if(queuemgr->pushindex != queuemgr->popindex)
		// 	{

		// 	}
		// 	pthread_spin_unlock(&(queuemgr->lock));
		// }
		
		if(info->sendqueue.pushindex != info->sendqueue.popindex)
		{
			QueueManager* queuemgr = &(info->sendqueue);

			printf("rfidThread(20-%s), push: %d, pop: %d \n", 
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
					printf("rfidThread(21-%s), send to rfid error : %d != %d, err : %d, push: %d, pop: %d \n", 
							info->name, len, ptr->len, errno, queuemgr->pushindex, queuemgr->popindex);
					notifyresult(info, "ERROR05");
				}
				else
				{
					printf("rfidThread(22-%s), send to rfid:%d, push: %d, pop: %d \n", 
						info->name, len, queuemgr->pushindex, queuemgr->popindex);

					usleep(1000 * 100);
				}
			}
			else
			{
				printf("rfidThread(23-%s), no any data to send, push: %d, pop: %d \n", 
						info->name, queuemgr->pushindex, queuemgr->popindex);
			}
			
		}
		
		usleep(1000 * 20);
   } 

	close(info->fd);
	info->fd = -1;
}
