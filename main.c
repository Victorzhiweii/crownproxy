#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>  /* netdb is necessary for struct hostent */
#include <pthread.h> 
// #include "tcpserver.h"
#include "sock.h"
#include "uart.h" 
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "common.h"

 
static const char* AppVERSION = "20190308.001";

static DeviceContainerInfo* gDeviceContainerInfo;

/*static const char PLC_COMMAND_LIST[][30] = {
	"READ01",		// plc通知armbox读取载具码，并保存到gTempRfidBuffer中。
	"PICK01",		//armbox会把缓存gTempRfidBuffer中的码发送给PLC，并保存到RFID的缓存中
	"TEST01",		// plc通知armbox把RFID缓存中的码发送给macmini
	"RETEST01",		// 1号机进行重测
	"RETEST02",		// 2号机进行重测
	"RETEST03",		// 3号机进行重测
	"RTST0102"		// 从1号机转到2号机测试
	"RTST0103"		// 从1号机转到3号机测试	
	"RTST0201"		// 从2号机转到1号机测试
	"RTST0203"		// 从2号机转到3号机测试	
	"RTST0301"		// 从3号机转到1号机测试
	"RTST0302"		// 从3号机转到2号机测试	
	// 上面的命令必须是在plc的主连接里面发送，不能放在plcmac子连接中发送
	"TEST_ING",		// 正在测试中
	"TEST__OK",		//macmini测试结果为PASS
	"TEST__NG",		//macmini测试结果为FAIL
	"ERROR01",  // rfid 初始化失败
	"ERROR02",  // macmini01 联机失败
	"ERROR03",  // macmini02 联机失败
	"ERROR04",  // macmini03 联机失败
	"ERROR05",  // rfid 写命令失败
	"ERROR_06",  // rfid 读取失败
	"ERROR_07",  // rfid 读取校验失败
	"ERROR08",  // rfid 读取超时
	"ERROR_09",  // rfid 读取数据无效
	"ERROR_10",  // socket 发送数据失败
	//"ERROR11",  // socket mini response busy
	"ERROR_12",  // socket mini response ERROR
	"ERROR13",  // 没有数据可以用来发送
	"ERROR_14", // macmini回复的数据格式不对
	"ERROR_15", // 没有载具码数据
	"ERROR_16", // 不支持的PLC命令
	"ERROR_17",  // macmini 未联机
	"ERROR_18",  // test 未联机
};*/

//static char[100] gTempRfidBuffer;

static void dumpbufferinfo(char* buffer, int buffer_length)
{
	int i;
	for(i = 0; i < buffer_length; i++)
	{
		if(buffer[i] == '\0')
		{
			break;
		}

		if(buffer[i] >= 32 && buffer[i] < 127)
		{
			printf("%c ", buffer[i]);
		}
		else
		{
			printf("0x%02X ", buffer[i]);
		}
	}
	printf ("\n");
}

static void fill_macmini_data(QueueBuffer* queuebuffer, char* buffer, int buff_len)
{
	int i=0;

	for(i=0; i<buff_len; i++)
	{
		if(buffer[i] == ';')
		{
			queuebuffer->len = strlen("CASN:");
			memcpy(queuebuffer->buffer, "CASN:", queuebuffer->len);

			memcpy(queuebuffer->buffer + queuebuffer->len , buffer, i + 1);
			queuebuffer->len += i + 1;

			memcpy(queuebuffer->buffer + queuebuffer->len, "ASN:", strlen("ASN:"));
			queuebuffer->len += strlen("ASN:");
			
			int len = buff_len - (i + 1);
			if(len > 0)
			{
				memcpy(queuebuffer->buffer + queuebuffer->len, buffer + i + 1, len);
				queuebuffer->len += len;
			}

			memcpy(queuebuffer->buffer + queuebuffer->len, "\r\n", strlen("\r\n"));
			queuebuffer->len += strlen("\r\n");
			break;
		}
	}
}

static QueueBuffer gTmpRfidbuffer;

static int plc_data_recv(char* name, int fd, char* buffer, int buffer_length)
{
	// int i = 0;
	if(buffer == NULL)
	{
		printf("plc_data_recv(0): %s , len: %d, buffer is null\n", name, buffer_length);
		return 0;
	}

	printf("plc_data_recv(1): %s , len: %d, cmd: \n", name, buffer_length);
#if 1
	dumpbufferinfo(buffer, buffer_length);
#endif
#if 1
	QueueManager* queuemgr = NULL;
	QueueBuffer tmpbuffer;
	QueueManager* queuemgr2 = NULL;
	QueueBuffer tmpbuffer2;
	//tmpbuffer.len = 0;
	memset(&tmpbuffer, 0, sizeof(QueueBuffer));
	if(memcmp(buffer, "READ", strlen("READ")) == 0)
	{ // 
		queuemgr = &(gDeviceContainerInfo->RFID->sendqueue);
		tmpbuffer.len = sizeof(send_rfid_cmd_read_40);
		memcpy(tmpbuffer.buffer, send_rfid_cmd_read_40, sizeof(send_rfid_cmd_read_40));
	}
	else if(memcmp(buffer, "PICK", strlen("PICK")) == 0)
	{	// 收到PICK指令，armbox会把之前读到的码发送给PLC
		//queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
		
		// 拷贝数据到本地缓存
		if(gTmpRfidbuffer.len > 0)
		{
			memcpy(gDeviceContainerInfo->RFID->buf, &gTmpRfidbuffer.buffer, gTmpRfidbuffer.len);
			gDeviceContainerInfo->RFID->buf[gTmpRfidbuffer.len] = '\0';
		}
		else
		{
			// 通知plc主连接，没有rfid的数据
			queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
			strcpy(tmpbuffer.buffer, "ERROR_15");
			tmpbuffer.len = strlen(tmpbuffer.buffer);
		}
		
		gDeviceContainerInfo->RFID->len = gTmpRfidbuffer.len;
		// 理清缓存，避免下次产品码重复
		gTmpRfidbuffer.len = 0;
	}
#if 1
	else
	{
		SocketInfo* dev_dest = NULL;
		SocketInfo* test = NULL;
		if(memcmp(buffer, "TEST", strlen("TEST")) == 0)
		{
			if(gDeviceContainerInfo->RFID->len > 0)
			{
				fill_macmini_data(&tmpbuffer, gDeviceContainerInfo->RFID->buf, gDeviceContainerInfo->RFID->len);

				if(memcmp(buffer, "TEST01", strlen("TEST01")) == 0)
				{
					dev_dest = gDeviceContainerInfo->MacMini1;
					test = gDeviceContainerInfo->Test1;
					
				}
				else if(memcmp(buffer, "TEST02", strlen("TEST02")) == 0)
				{
					dev_dest = gDeviceContainerInfo->MacMini2;
					test = gDeviceContainerInfo->Test2;
				}
				else if(memcmp(buffer, "TEST03", strlen("TEST03")) == 0)
				{
					dev_dest = gDeviceContainerInfo->MacMini3;
					test = gDeviceContainerInfo->Test3;
				}
				
				if(dev_dest == NULL)
				{
					printf("plc_data_recv(2): %s , %d, invalid plc command \n", name, buffer_length);
					// 通知plc主连接，该命令行不支持
					queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
					strcpy(tmpbuffer.buffer, "ERROR_16");
					tmpbuffer.len = strlen(tmpbuffer.buffer);
				}
				else 
				{
					dev_dest->isretesting = 0;

					if(dev_dest->connected == 0)
					{
						// 通知plc主连接，mac mini 未连接
						queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
						strcpy(tmpbuffer.buffer, "ERROR_17");
						tmpbuffer.len = strlen(tmpbuffer.buffer);
					}
					else 
					{
						queuemgr = &(dev_dest->sendqueue);
						// 保存当前测试的缓存
						memcpy(dev_dest->buf, tmpbuffer.buffer, tmpbuffer.len+1);
						dev_dest->len = tmpbuffer.len;
						gDeviceContainerInfo->RFID->len = 0;
					}
				}
				
				if(test != NULL)
				{
					if(test->connected == 0)
					{
						// 通知plc主连接，test 未连接
						queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
						strcpy(tmpbuffer.buffer, "ERROR_18");
						tmpbuffer.len = strlen(tmpbuffer.buffer);
					}
					else 
					{
						queuemgr2 = &(test->sendqueue);
						strcpy(tmpbuffer2.buffer, "CLOSE");
						tmpbuffer2.len = strlen(tmpbuffer2.buffer);
					}
				}
			}
			else
			{
				// 通知plc主连接，没有rfid的数据
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_15");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
		}
		else if(memcmp(buffer, "RETEST", strlen("RETEST")) == 0)
		{
			if(memcmp(buffer, "RETEST01", strlen("RETEST01")) == 0)
			{
				dev_dest = gDeviceContainerInfo->MacMini1;
				test = gDeviceContainerInfo->Test1;
			}
			else if(memcmp(buffer, "RETEST02", strlen("RETEST02")) == 0)
			{
				dev_dest = gDeviceContainerInfo->MacMini2;
				test = gDeviceContainerInfo->Test2;
			}
			else if(memcmp(buffer, "RETEST03", strlen("RETEST03")) == 0)
			{
				dev_dest = gDeviceContainerInfo->MacMini3;
				test = gDeviceContainerInfo->Test3;
			}

			if(dev_dest == NULL)
			{
				printf("plc_data_recv(3): %s , %d, invalid plc command \n", name, buffer_length);
				
				// 通知plc主连接，该命令行不支持
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_16");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			else if(dev_dest->connected == 0)
			{
				// 通知plc主连接，mac mini 未连接
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_17");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			else if(dev_dest->len > 0)
			{
				queuemgr = &(dev_dest->sendqueue);

				memcpy(tmpbuffer.buffer, dev_dest->buf, dev_dest->len);
				tmpbuffer.len = dev_dest->len;

				// 设置为重测标志
				dev_dest->isretesting = 1;
			}
			else
			{
				// 通知plc主连接，没有rfid的数据
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_15");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			
			if(test != NULL)
			{
				
				if(test->connected == 0)
				{
					// 通知plc主连接，test 未连接
					queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
					strcpy(tmpbuffer.buffer, "ERROR_18");
					tmpbuffer.len = strlen(tmpbuffer.buffer);
				}
				else 
				{
					queuemgr2 = &(test->sendqueue);
					strcpy(tmpbuffer2.buffer, "CLOSE");
					tmpbuffer2.len = strlen(tmpbuffer2.buffer);
				}
			}
			
		}
		else if(memcmp(buffer, "RTST", strlen("RTST")) == 0)
		{
			SocketInfo* dev_source = NULL;
			if(memcmp(buffer, "RTST0102", strlen("RTST0102")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini1;
				dev_dest = gDeviceContainerInfo->MacMini2;	
				test = gDeviceContainerInfo->Test2;				
			}
			else if(memcmp(buffer, "RTST0103", strlen("RTST0103")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini1;
				dev_dest = gDeviceContainerInfo->MacMini3;
				test = gDeviceContainerInfo->Test3;					
			}
			else if(memcmp(buffer, "RTST0201", strlen("RTST0201")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini2;
				dev_dest = gDeviceContainerInfo->MacMini1;	
				test = gDeviceContainerInfo->Test1;	
			}
			else if(memcmp(buffer, "RTST0203", strlen("RTST0203")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini2;
				dev_dest = gDeviceContainerInfo->MacMini3;	
				test = gDeviceContainerInfo->Test3;	
			}
			else if(memcmp(buffer, "RTST0301", strlen("RTST0301")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini3;
				dev_dest = gDeviceContainerInfo->MacMini1;	
				test = gDeviceContainerInfo->Test1;	
			}
			else if(memcmp(buffer, "RTST0302", strlen("RTST0302")) == 0)
			{
				dev_source = gDeviceContainerInfo->MacMini3;
				dev_dest = gDeviceContainerInfo->MacMini2;	
				test = gDeviceContainerInfo->Test2;	
			}

			if(dev_source == NULL || dev_dest == NULL)
			{
				printf("plc_data_recv(4): %s , %d, invalid plc command \n", name, buffer_length);
				
				// 通知plc主连接，该命令行不支持
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_16");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			else if(dev_dest->connected == 0)
			{
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_17");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			else if(dev_source->len > 0)
			{
				queuemgr = &(dev_dest->sendqueue);

				memcpy(tmpbuffer.buffer, dev_source->buf, dev_source->len);
				tmpbuffer.len = dev_source->len;

				memcpy(dev_dest->buf, tmpbuffer.buffer, tmpbuffer.len+1);
				dev_dest->len = tmpbuffer.len;

				// 设置为重测标志
				dev_dest->isretesting = 1;

				// 清除原始macmini测试机器的缓存码
				dev_source->len = 0;
			}
			else
			{
				// 通知plc主连接，没有rfid的数据
				queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
				strcpy(tmpbuffer.buffer, "ERROR_15");
				tmpbuffer.len = strlen(tmpbuffer.buffer);
			}
			
			if(test != NULL)
			{
				
				if(test->connected == 0)
				{
					// 通知plc主连接，test 未连接
					queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
					strcpy(tmpbuffer.buffer, "ERROR_18");
					tmpbuffer.len = strlen(tmpbuffer.buffer);
				}
				else 
				{
					queuemgr2 = &(test->sendqueue);
					strcpy(tmpbuffer2.buffer, "CLOSE");
					tmpbuffer2.len = strlen(tmpbuffer2.buffer);
				}
			}
			
		}
		else
		{
			printf("plc_data_recv(5): %s , %d, invalid plc command:", name, buffer_length);
			dumpbufferinfo(buffer, buffer_length);

			// 通知plc主连接，该命令行不支持
			queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
			strcpy(tmpbuffer.buffer, "ERROR_16");
			tmpbuffer.len = strlen(tmpbuffer.buffer);
		}
	}
	
#else
	else if(memcmp(buffer, "TEST01", strlen("TEST01")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini1->sendqueue);
		if(gDeviceContainerInfo->RFID->len > 0)
		{
			fill_macmini_data(&tmpbuffer, gDeviceContainerInfo->RFID->buf, gDeviceContainerInfo->RFID->len);

			// 保存当前测试的缓存
			memcpy(gDeviceContainerInfo->MacMini1->buf, tmpbuffer.buffer, tmpbuffer.len+1);
			gDeviceContainerInfo->RFID->len = 0;

			gDeviceContainerInfo->MacMini1->len = tmpbuffer.len;
		}
		gDeviceContainerInfo->MacMini1->len = tmpbuffer.len;
	}
	else if(memcmp(buffer, "TEST02", strlen("TEST02")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini2->sendqueue);
		if(gDeviceContainerInfo->RFID->len > 0)
		{
			fill_macmini_data(&tmpbuffer, gDeviceContainerInfo->RFID->buf, gDeviceContainerInfo->RFID->len);

			// 保存当前测试的缓存
			memcpy(gDeviceContainerInfo->MacMini2->buf, tmpbuffer.buffer, tmpbuffer.len + 1);			
		}
		gDeviceContainerInfo->MacMini2->len = tmpbuffer.len;
	}
	else if(memcmp(buffer, "TEST03", strlen("TEST03")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini3->sendqueue);
		if(gDeviceContainerInfo->RFID->len > 0)
		{
			fill_macmini_data(&tmpbuffer, gDeviceContainerInfo->RFID->buf, gDeviceContainerInfo->RFID->len);

			// 保存当前测试的缓存
			memcpy(gDeviceContainerInfo->MacMini3->buf, tmpbuffer.buffer, tmpbuffer.len+1);			
		}
		gDeviceContainerInfo->MacMini3->len = tmpbuffer.len;
	}
	else if(memcmp(buffer, "RETEST01", strlen("RETEST01")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini1->sendqueue);
		if(gDeviceContainerInfo->MacMini1->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini1->buf, gDeviceContainerInfo->MacMini1->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini1->len;
		}
	}
	else if(memcmp(buffer, "RETEST02", strlen("RETEST02")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini2->sendqueue);
		if(gDeviceContainerInfo->MacMini2->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini2->buf, gDeviceContainerInfo->MacMini2->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini2->len;
		}
	}
	else if(memcmp(buffer, "RETEST03", strlen("RETEST03")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini3->sendqueue);
		if(gDeviceContainerInfo->MacMini3->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini3->buf, gDeviceContainerInfo->MacMini3->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini3->len;
		}
	}
	else if(memcmp(buffer, "RTST0102", strlen("RTST0102")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini2->sendqueue);
		if(gDeviceContainerInfo->MacMini1->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini1->buf, gDeviceContainerInfo->MacMini1->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini1->len;

			memcpy(gDeviceContainerInfo->MacMini2->buf, tmpbuffer.buffer, tmpbuffer.len+1);
			gDeviceContainerInfo->MacMini2->len = tmpbuffer.len;
		}
	}
	else if(memcmp(buffer, "RTST0103", strlen("RTST0103")) == 0)
	{
		queuemgr = &(gDeviceContainerInfo->MacMini3->sendqueue);
		if(gDeviceContainerInfo->MacMini1->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini1->buf, gDeviceContainerInfo->MacMini1->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini1->len;

			memcpy(gDeviceContainerInfo->MacMini3->buf, tmpbuffer.buffer, tmpbuffer.len+1);
			gDeviceContainerInfo->MacMini3->len = tmpbuffer.len;
		}
	}
	else if(memcmp(buffer, "RTST0201", strlen("RTST0201")) == 0)
	{
		// 需要发送的目标机器
		queuemgr = &(gDeviceContainerInfo->MacMini1->sendqueue);

		// 把原始的数据拷贝到的目标机器
		if(gDeviceContainerInfo->MacMini2->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini2->buf, gDeviceContainerInfo->MacMini2->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini2->len;

			memcpy(gDeviceContainerInfo->MacMini1->buf, tmpbuffer.buffer, tmpbuffer.len+1);
			gDeviceContainerInfo->MacMini1->len = tmpbuffer.len;
		}
	}
	else if(memcmp(buffer, "RTST0203", strlen("RTST0203")) == 0)
	{
		// 需要发送的目标机器
		queuemgr = &(gDeviceContainerInfo->MacMini3->sendqueue);

		// 把原始的数据拷贝到的目标机器
		if(gDeviceContainerInfo->MacMini2->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini2->buf, gDeviceContainerInfo->MacMini2->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini2->len;

			memcpy(gDeviceContainerInfo->MacMini3->buf, tmpbuffer.buffer, tmpbuffer.len+1);
			gDeviceContainerInfo->MacMini3->len = tmpbuffer.len;
		}
	}
	else if(memcmp(buffer, "RTST0301", strlen("RTST0301")) == 0)
	{
		// 需要发送的目标机器
		queuemgr = &(gDeviceContainerInfo->MacMini1->sendqueue);

		// 把原始的数据拷贝到的目标机器
		if(gDeviceContainerInfo->MacMini3->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini3->buf, gDeviceContainerInfo->MacMini3->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini3->len;

			memcpy(gDeviceContainerInfo->MacMini1->buf, tmpbuffer.buffer, tmpbuffer.len+1);			
		}
		gDeviceContainerInfo->MacMini1->len = tmpbuffer.len;
	}
	else if(memcmp(buffer, "RTST0302", strlen("RTST0302")) == 0)
	{
		// 需要发送的目标机器
		queuemgr = &(gDeviceContainerInfo->MacMini2->sendqueue);

		// 把原始的数据拷贝到的目标机器
		if(gDeviceContainerInfo->MacMini3->len > 0)
		{
			memcpy(tmpbuffer.buffer, gDeviceContainerInfo->MacMini3->buf, gDeviceContainerInfo->MacMini3->len);
			tmpbuffer.len = gDeviceContainerInfo->MacMini3->len;

			memcpy(gDeviceContainerInfo->MacMini2->buf, tmpbuffer.buffer, tmpbuffer.len+1);			
		}
		gDeviceContainerInfo->MacMini2->len = tmpbuffer.len;
	}
	else
	{
		printf("plc_data_recv(4): %s , %d, invalid plc command:", name, buffer_length);
		for(i = 0; i < buffer_length; i++)
		{
			if(buffer[i] == '\0')
			{
				break;
			}

			if(buffer[i] >= 32 && buffer[i] < 127)
			{
				printf("%c ", buffer[i]);
			}
			else
			{
				printf("0x%02X ", buffer[i]);
			}
		}
		printf ("\n");

		// 通知plc主连接，该命令行不支持
		queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
		strcpy(tmpbuffer.buffer, "ERROR_16");
		tmpbuffer.len = strlen(tmpbuffer.buffer);
	}
#endif

	if(queuemgr != NULL)
	{
		pthread_spin_lock(&(queuemgr->lock));
		int index = queuemgr->pushindex;
		if(tmpbuffer.len > 0)
		{
			memcpy(&(queuemgr->buffer[index]), &tmpbuffer, sizeof(tmpbuffer));

			index++;
			if(index >= MAX_SEND_BUFFER_COUNT)
			{
				index = 0;
			}

			#if 1
			printf("plc_data_recv(7): %s , len: %d, send data:", name, tmpbuffer.len);
			dumpbufferinfo(tmpbuffer.buffer, tmpbuffer.len);			
			#endif

			queuemgr->pushindex = index;
		}
		else
		{
			memset(&(queuemgr->buffer[index]), 0, sizeof(QueueBuffer));

			printf("plc_data_recv(8): %s , %d, no any data to send:", name, buffer_length);
			dumpbufferinfo(buffer, buffer_length);
		}
		
		pthread_spin_unlock(&(queuemgr->lock));
	}
	
	if(queuemgr2 != NULL)
	{
		pthread_spin_lock(&(queuemgr2->lock));
		int index2 = queuemgr2->pushindex;
		if(tmpbuffer2.len > 0)
		{
			memcpy(&(queuemgr2->buffer[index2]), &tmpbuffer2, sizeof(tmpbuffer2));

			index2++;
			if(index2 >= MAX_SEND_BUFFER_COUNT)
			{
				index2 = 0;
			}

			#if 1
			printf("plc_data_recv(7): %s , len: %d, send data:", name, tmpbuffer2.len);
			dumpbufferinfo(tmpbuffer2.buffer, tmpbuffer2.len);			
			#endif

			queuemgr2->pushindex = index2;
		}
		else
		{
			memset(&(queuemgr2->buffer[index2]), 0, sizeof(QueueBuffer));

			printf("plc_data_recv(8): %s , %d, no any data to send:", name, buffer_length);
			dumpbufferinfo(buffer, buffer_length);
		}
		
		pthread_spin_unlock(&(queuemgr2->lock));
	}
#else
	for(i=0; i < sizeof(PLC_COMMAND_LIST) / sizeof(PLC_COMMAND_LIST[0]); i++)
	{
		if(strncmp(buffer, PLC_COMMAND_LIST[i], strlen(PLC_COMMAND_LIST[i])) == 0)
		{
		}
	}
#endif
	
	return 0;
}

static int rfid_data_recv(char* name, int fd, char* buffer, int buffer_length)
{
	// int i = 0;
	if(buffer == NULL)
	{
		printf("rfid_data_recv(0): %s , len: %d, buffer is null\n", name, buffer_length);
		return 0;
	}

	printf("rfid_data_recv(1): %s , read data numbytes = %d, content:\n", name, buffer_length);
	#if 1
		dumpbufferinfo(buffer, buffer_length);
	#endif

	// 拷贝数据到本地缓存
	#if 1
	//memcpy(gTempRfidBuffer, buffer, buffer_length);
	//gTempRfidBuffer[buffer_length] = '\0';
	if(buffer_length > 0)
	{
		memcpy(gTmpRfidbuffer.buffer, buffer, buffer_length);
		gTmpRfidbuffer.buffer[buffer_length] = '\0';

		gTmpRfidbuffer.len = buffer_length;
	}
	else
	{
		memset(&gTmpRfidbuffer, 0, sizeof(gTmpRfidbuffer));
	}
	
#if 1	// 如果有错误发生，就发送ERROR信息给PLC，否则不会发送任何信息给PLC
	//if(strncmp(buffer, "ERROR", strlen("ERROR")) == 0)
	if(gTmpRfidbuffer.len > 0)
	{
		// 将数据拷贝到plc的发送队列
		QueueManager* queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
		pthread_spin_lock(&(queuemgr->lock));
		int index = queuemgr->pushindex;
		
		memcpy(&(queuemgr->buffer[index]), &gTmpRfidbuffer, sizeof(gTmpRfidbuffer));
		
		index++;
		if(index >= MAX_SEND_BUFFER_COUNT)
		{
			index = 0;
		}
		
		queuemgr->pushindex = index;

		pthread_spin_unlock(&(queuemgr->lock));
	}
#endif	
	#else
	memcpy(gDeviceContainerInfo->RFID->buf, buffer, buffer_length);
	gDeviceContainerInfo->RFID->buf[buffer_length] = '\0';
	gDeviceContainerInfo->RFID->len = buffer_length;

	// 将数据拷贝到plc的发送队列
	QueueManager* queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
	pthread_spin_lock(&(queuemgr->lock));
	int index = queuemgr->pushindex;

	memcpy(queuemgr->buffer[index].buffer, gDeviceContainerInfo->RFID->buf, buffer_length + 1);
	//strcpy(queuemgr->buffer[index].buffer, gDeviceContainerInfo->RFID->buf);
	queuemgr->buffer[index].len = buffer_length;

	index++;
	if(index >= MAX_SEND_BUFFER_COUNT)
	{
		index = 0;
	}
	
	queuemgr->pushindex = index;

	pthread_spin_unlock(&(queuemgr->lock));
	#endif
	return 0;
}

static int macmini_data_recv(char* name, int fd, char* buffer, int buffer_length)
{
	int i = 0;
	if(buffer == NULL)
	{
		printf("macmini_data_recv(2): %s , len: %d, buffer is null\n", name, buffer_length);
		return 0;
	}
	printf("macmini_data_recv(1): %s , len: %d, content:\n", name, buffer_length);	
	#if 1
		dumpbufferinfo(buffer, buffer_length);
	#endif

	QueueBuffer tmpbuffer;
	QueueBuffer tmpbuffer2;
	QueueManager* queuemgr =  NULL;
	QueueManager* queuemgr2 =  NULL;
	SocketInfo* test = NULL;
	
	
	memset(&tmpbuffer, 0, sizeof(QueueBuffer));
	if(memcmp(buffer, "BUSY", strlen("BUSY")) == 0)
	{
		printf("macmini_data_recv(2): %s, mac mini response busy\n", name);

		strcpy(tmpbuffer.buffer, "TEST_ING");
		tmpbuffer.len = strlen(tmpbuffer.buffer);
		tmpbuffer.buffer[tmpbuffer.len] = '\0';
	}
	else if(memcmp(buffer, "ERROR", strlen("ERROR")) == 0)
	{
		printf("macmini_data_recv(2): %s, mac mini response ERROR_12\n", name);

		strcpy(tmpbuffer.buffer, "ERROR_12");
		tmpbuffer.len = strlen(tmpbuffer.buffer);
		tmpbuffer.buffer[tmpbuffer.len] = '\0';
	}
	else if(memcmp(buffer, "ASN:", strlen("ASN:")) == 0)
	{
		for(i = 0; i < buffer_length; i++)
		{
			if(buffer[i] == ',')
			{
				if(buffer[i+1] == 'P')
				{
					strcpy(tmpbuffer.buffer, "TEST__OK");

					// 判断是否是重复测试
					if(strcmp(name, "MAC01") == 0){
						
						if(gDeviceContainerInfo->MacMini1->isretesting == 1)
						{
							
							strcpy(tmpbuffer.buffer, "RETESTOK");
						}
						test = gDeviceContainerInfo->Test1;
					}
					else if(strcmp(name, "MAC02") == 0){
					
						if(gDeviceContainerInfo->MacMini2->isretesting == 1)
						{
							
							strcpy(tmpbuffer.buffer, "RETESTOK");
						}
						test = gDeviceContainerInfo->Test2;
					}
					else if(strcmp(name, "MAC03") == 0){
					
						if(gDeviceContainerInfo->MacMini3->isretesting == 1)
						{
							
							strcpy(tmpbuffer.buffer, "RETESTOK");
						}
						test = gDeviceContainerInfo->Test3;
					}
				}
				else
				{
					strcpy(tmpbuffer.buffer, "TEST__NG");
					
					// 判断是否是重复测试
					if(strcmp(name, "MAC01") == 0){
						
						if(gDeviceContainerInfo->MacMini1->isretesting == 1)
						{
							strcpy(tmpbuffer.buffer, "RETESTNG");
						}
						test = gDeviceContainerInfo->Test1;
					}
					else if(strcmp(name, "MAC02") == 0){
						
						if(gDeviceContainerInfo->MacMini2->isretesting == 1)
						{
							strcpy(tmpbuffer.buffer, "RETESTNG");
						}
						test = gDeviceContainerInfo->Test2;
					}
					else if(strcmp(name, "MAC03") == 0){
						
						if(gDeviceContainerInfo->MacMini3->isretesting == 1)
						{
							strcpy(tmpbuffer.buffer, "RETESTNG");
						}
						test = gDeviceContainerInfo->Test3;
					}
				}

				tmpbuffer.len = strlen(tmpbuffer.buffer);
				break;
			}
		}
		
		if(tmpbuffer.len == 0)
		{	// macmini回复的数据格式不对
			strcpy(tmpbuffer.buffer, "ERROR_14");
			tmpbuffer.len = strlen(tmpbuffer.buffer);
		}
		
		
	}
	else
	{
		printf("macmini_data_recv(3): %s, mac mini response unkown data:\n", name);
		#if 1
			dumpbufferinfo(buffer, buffer_length);
		#endif
		// macmini回复的数据格式不对
		strcpy(tmpbuffer.buffer, "ERROR_14");
		tmpbuffer.len = strlen(tmpbuffer.buffer);
	}
	
	if(test != NULL)
	{
		
		if(test->connected == 0)
		{
			// 通知plc主连接，mac mini 未连接
			queuemgr2 = &(gDeviceContainerInfo->PLC->sendqueue);
			strcpy(tmpbuffer2.buffer, "ERROR_18");
			tmpbuffer2.len = strlen(tmpbuffer2.buffer);
		}
		else 
		{
			queuemgr2 = &(test->sendqueue);
			strcpy(tmpbuffer2.buffer, "OPEN");
			tmpbuffer2.len = strlen(tmpbuffer2.buffer);
		}
	}

	if(tmpbuffer.len > 0)
	{
		
		// 将数据拷贝到plc的发送队列
		if(gDeviceContainerInfo->PLC_Mac1 != NULL && strcmp(name, "MAC01") == 0)
		{
			queuemgr = &(gDeviceContainerInfo->PLC_Mac1->sendqueue);
		}
		else if(gDeviceContainerInfo->PLC_Mac2 != NULL && strcmp(name, "MAC02") == 0)
		{
			queuemgr = &(gDeviceContainerInfo->PLC_Mac2->sendqueue);
		}
		else if(gDeviceContainerInfo->PLC_Mac3 != NULL && strcmp(name, "MAC03") == 0)
		{
			queuemgr = &(gDeviceContainerInfo->PLC_Mac3->sendqueue);
		}
		else
		{
			queuemgr = &(gDeviceContainerInfo->PLC->sendqueue);
		}
		
		if(queuemgr == NULL)
		{
			return 1;
		}

		pthread_spin_lock(&(queuemgr->lock));
		int index = queuemgr->pushindex;

		memcpy(&(queuemgr->buffer[index]), &tmpbuffer, sizeof(tmpbuffer));

		index++;
		if(index >= MAX_SEND_BUFFER_COUNT)
		{
			index = 0;
		}
		
		queuemgr->pushindex = index;

		pthread_spin_unlock(&(queuemgr->lock));
	}
	else
	{
		printf("macmini_data_recv(8): %s, mac mini response no valid data.\n", name);
	}
	
	if(queuemgr2 != NULL)
	{
		pthread_spin_lock(&(queuemgr2->lock));
		int index2 = queuemgr2->pushindex;
		if(tmpbuffer2.len > 0)
		{
			memcpy(&(queuemgr2->buffer[index2]), &tmpbuffer2, sizeof(tmpbuffer2));

			index2++;
			if(index2 >= MAX_SEND_BUFFER_COUNT)
			{
				index2 = 0;
			}

			#if 1
			printf("plc_data_recv(7): %s , len: %d, send data:", name, tmpbuffer2.len);
			dumpbufferinfo(tmpbuffer2.buffer, tmpbuffer2.len);			
			#endif

			queuemgr2->pushindex = index2;
		}
		else
		{
			memset(&(queuemgr2->buffer[index2]), 0, sizeof(QueueBuffer));

			printf("plc_data_recv(8): %s , %d, no any data to send:", name, buffer_length);
			dumpbufferinfo(buffer, buffer_length);
		}
		
		pthread_spin_unlock(&(queuemgr2->lock));
	}
	
	return 0;
}

static void fill_socket_profile(SocketInfo* info, cJSON *data)
{
	cJSON *parameter;
	char* addr;
	char *key;
	int z, child_count;
	//printf("fill_socket_profile(0) name : %s \n", info->name);
	//format = cJSON_GetObjectItem(data, key);
	child_count = cJSON_GetArraySize(data);
	//printf("fill_socket_profile(1) child_count %d \n", child_count);
	for(z = 0; z < child_count; z++)
	{
		parameter = cJSON_GetArrayItem(data,z);	
		key = parameter->string;

		//printf("fill_socket_profile(2) index: %d, key:%s \n", z, key);
		
		if(strcmp(key,"ip") == 0)
		{
			addr = cJSON_GetObjectItem(data,key)->valuestring;
			//printf("fill_socket_profile(4) index: %d, value:%s \n", z, addr);
			strcpy(info->ip, addr);
		}
		else if(strcmp(key,"port") == 0)
		{
			info->port = cJSON_GetObjectItem(data,key)->valueint;	

			//printf("fill_socket_profile(4) index: %d, value:%d \n", z, info->port);
		}
	}
}

static void fill_uart_profile(SerialPortInfo* info, cJSON *data)
{
	// cJSON *format;
	cJSON *parameter;
	char* addr;
	char *key;
	int z, child_count;
	//format = cJSON_GetObjectItem(data, key);
	//printf("fill_uart_profile(0) name : %s \n", info->name);
	
	child_count = cJSON_GetArraySize(data);
	//printf("fill_uart_profile(1) child_count %d \n", child_count);
	for(z = 0; z < child_count; z++)
	{
		parameter = cJSON_GetArrayItem(data,z);	
		key = parameter->string;
		//printf("fill_uart_profile(2) index: %d, key:%s \n", z, key);
		if(strcmp(key,"dev") == 0)
		{
			addr = cJSON_GetObjectItem(data,key)->valuestring;
			//port = cJSON_GetObjectItem(format,"port")->valueint;	
			//printf("fill_uart_profile(3) index: %d, value:%s \n", z, addr);
			strcpy(info->dev, addr);
		}
		else if(strcmp(key,"speed") == 0)
		{						
			//addr = cJSON_GetObjectItem(format,key1)->valuestring;
			//strcpy(addr, RfidInfo.dev);
			
			info->speed = cJSON_GetObjectItem(data,key)->valueint;	

			//printf("fill_uart_profile(4) index: %d, value:%d \n", z, info->speed);
		}
	}
}

int main(int argc, const char *argv[])
{
	struct stat wordbuf;
	
	FILE *fp;
	long len;
	char *content = NULL;
	cJSON *json;
	cJSON *pSub;
	cJSON *format;
	// cJSON *parameter;
	char *key;
	int parameter_size;
	// int format_size;
	int i;
	
	int wordfd;
	// long now_time;
	long modify_time;

	// DIR *dirp; 
    // struct dirent *dp;
	// int now_USB0 = 0;
	// int last_USB0 = 1;
	// int flag_USB = 0;

	SerialPortInfo RfidInfo;
	SocketInfo PlcInfo;
	SocketInfo PlcMac1Info;
	SocketInfo PlcMac2Info;
	SocketInfo PlcMac3Info;
	SocketInfo MacMini1;
	SocketInfo MacMini2;
	SocketInfo MacMini3;
	SocketInfo Test1;
	SocketInfo Test2;
	SocketInfo Test3;
	

	DeviceContainerInfo device_info;
	device_info.RFID = &RfidInfo;
	device_info.PLC = &PlcInfo;
	device_info.PLC_Mac1 = &PlcMac1Info;
	device_info.PLC_Mac2 = &PlcMac2Info;
	device_info.PLC_Mac3 = &PlcMac3Info;
	device_info.MacMini1 = &MacMini1;
	device_info.MacMini2 = &MacMini2;
	device_info.MacMini3 = &MacMini3;
	device_info.Test1 = &Test1;
	device_info.Test2 = &Test2;
	device_info.Test3 = &Test3;
	gDeviceContainerInfo = &device_info;

	memset(device_info.RFID, 0, sizeof(SerialPortInfo));
	memset(device_info.PLC, 0, sizeof(SocketInfo));
	memset(device_info.PLC_Mac1, 0, sizeof(SocketInfo));
	memset(device_info.PLC_Mac2, 0, sizeof(SocketInfo));
	memset(device_info.PLC_Mac3, 0, sizeof(SocketInfo));
	memset(device_info.MacMini1, 0, sizeof(SocketInfo));
	memset(device_info.MacMini2, 0, sizeof(SocketInfo));
	memset(device_info.MacMini3, 0, sizeof(SocketInfo));
	memset(device_info.Test1, 0, sizeof(SocketInfo));
	memset(device_info.Test2, 0, sizeof(SocketInfo));
	memset(device_info.Test3, 0, sizeof(SocketInfo));
	
	RfidInfo.callback = rfid_data_recv;
	PlcInfo.callback = plc_data_recv;
	PlcMac1Info.callback = plc_data_recv;
	PlcMac2Info.callback = plc_data_recv;
	PlcMac3Info.callback = plc_data_recv;
	MacMini1.callback = macmini_data_recv;
	MacMini2.callback = macmini_data_recv;
	MacMini3.callback = macmini_data_recv;

	/* 
		参数个数小于1则返回，按如下方式执行:
		./uart_test /dev/ttyAT1
	*/
	if (argc < 2) {
		printf("Useage: %s no parameter\n", argv[0]);
		//exit(0);
    } 

	printf("app version:%s\n", AppVERSION);
	if(argc > 1 && memcmp(argv[1], "-v", strlen("-v")) == 0)
	{
		exit(0);
		return 0;
	}

	fp=fopen("./crownconfig.json","rb");
	if(fp == NULL){
		printf("main(2): open crownconfig.json file error\n");
		exit(1);
		return 1;
	}

	

	wordfd=fileno(fp);
	fstat(wordfd, &wordbuf);
	modify_time=wordbuf.st_mtime; 
	
	fseek(fp,0,SEEK_END);
	len=ftell(fp);
	if(len > 0)
	{
		fseek(fp,0,SEEK_SET);
		content=(char*)malloc(len+1);
		fread(content,1,len,fp);
	}
	
	fclose(fp);

	if(len == 0){
		printf("main(3): config file: crownconfig.json  has no data\n");
		exit(2);
		return 2;
	}
	

	pthread_spin_init(&(RfidInfo.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(PlcInfo.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(PlcMac1Info.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(PlcMac2Info.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(PlcMac3Info.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(MacMini1.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(MacMini2.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(MacMini3.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);

	pthread_spin_init(&(Test1.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(Test2.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&(Test3.sendqueue.lock), PTHREAD_PROCESS_PRIVATE);
	
	json=cJSON_Parse(content);

	parameter_size = cJSON_GetArraySize(json);

	for (i = 0; i < parameter_size; i++)
	{
		pSub = cJSON_GetArrayItem(json,i);	
		key = pSub->string;
		//format = cJSON_GetObjectItem(json, key);
		printf("main(3) parse json key : %s \n", key);
		if(strcmp(key, "RFID") == 0)
		{	
			format = cJSON_GetObjectItem(json, "RFID");
			strcpy(RfidInfo.name, key);
			fill_uart_profile(&RfidInfo, format);

			printf("main(5) parse result: %s, %s, %d \n", RfidInfo.name, RfidInfo.dev, RfidInfo.speed);

			//rfidSerialPortThread(&RfidInfo);
			pthread_create(&(RfidInfo.handler),NULL,(void*)rfidSerialPortThread, &RfidInfo);
		}
		else if(strcmp(key, "PLC") == 0)
		{
			format = cJSON_GetObjectItem(json, "PLC");
			strcpy(PlcInfo.name, key);
			fill_socket_profile(&PlcInfo, format);

			printf("main(6) parse result: %s, %s, %d\n", PlcInfo.name, PlcInfo.ip, PlcInfo.port);
			//pthread_create(&(PlcInfo.handler),NULL,(void*)tcpSocketThread, &PlcInfo);
		}
		else if(strcmp(key, "PLC_MAC01") == 0)
		{
			format = cJSON_GetObjectItem(json, "PLC_MAC01");
			strcpy(PlcMac1Info.name, key);
			fill_socket_profile(&PlcMac1Info, format);

			printf("main(6) parse result: %s, %s, %d\n", PlcMac1Info.name, PlcMac1Info.ip, PlcMac1Info.port);
			pthread_create(&(PlcMac1Info.handler),NULL,(void*)tcpSocketThread, &PlcMac1Info);
		}
		else if(strcmp(key, "PLC_MAC02") == 0)
		{
			format = cJSON_GetObjectItem(json, "PLC_MAC02");
			strcpy(PlcMac2Info.name, key);
			fill_socket_profile(&PlcMac2Info, format);

			printf("main(6) parse result: %s, %s, %d\n", PlcMac2Info.name, PlcMac2Info.ip, PlcMac2Info.port);
			pthread_create(&(PlcMac2Info.handler),NULL,(void*)tcpSocketThread, &PlcMac2Info);
		}
		else if(strcmp(key, "PLC_MAC03") == 0)
		{
			format = cJSON_GetObjectItem(json, "PLC_MAC03");
			strcpy(PlcMac3Info.name, key);
			fill_socket_profile(&PlcMac3Info, format);

			printf("main(6) parse result: %s, %s, %d\n", PlcMac3Info.name, PlcMac3Info.ip, PlcMac3Info.port);
			pthread_create(&(PlcMac3Info.handler),NULL,(void*)tcpSocketThread, &PlcMac3Info);
		}
		else if(strcmp(key, "MAC01") == 0)
		{
			format = cJSON_GetObjectItem(json, "MAC01");
			strcpy(MacMini1.name, key);
			fill_socket_profile(&MacMini1, format);

			printf("main(7) parse result: %s, %s, %d\n", MacMini1.name, MacMini1.ip, MacMini1.port);

			pthread_create(&(MacMini1.handler),NULL,(void*)tcpSocketThread, &MacMini1);
		}
		else if(strcmp(key, "MAC02") == 0)
		{
			format = cJSON_GetObjectItem(json, "MAC02");
			strcpy(MacMini2.name, key);
			fill_socket_profile(&MacMini2, format);

			printf("main(8) parse result: %s, %s, %d\n", MacMini2.name, MacMini2.ip, MacMini2.port);

			pthread_create(&(MacMini2.handler),NULL,(void*)tcpSocketThread, &MacMini2);
		}
		else if(strcmp(key, "MAC03") == 0)
		{
			format = cJSON_GetObjectItem(json, "MAC03");
			strcpy(MacMini3.name, key);
			fill_socket_profile(&MacMini3, format);

			printf("main(9) parse result: %s, %s, %d\n", MacMini3.name, MacMini3.ip, MacMini3.port);

			
			pthread_create(&(MacMini3.handler),NULL,(void*)tcpSocketThread, &MacMini3);
		}
		else if(strcmp(key, "TEST01") == 0)
		{
			format = cJSON_GetObjectItem(json, "TEST01");
			strcpy(Test1.name, key);
			fill_socket_profile(&Test1, format);

			printf("main(9) parse result: %s, %s, %d\n", Test1.name, Test1.ip, Test1.port);

			
			pthread_create(&(Test1.handler),NULL,(void*)tcpSocketThread, &Test1);
		}
		else if(strcmp(key, "TEST02") == 0)
		{
			format = cJSON_GetObjectItem(json, "TEST02");
			strcpy(Test2.name, key);
			fill_socket_profile(&Test2, format);

			printf("main(9) parse result: %s, %s, %d\n", Test2.name, Test2.ip, Test2.port);

			
			pthread_create(&(Test2.handler),NULL,(void*)tcpSocketThread, &Test2);
		}
		else if(strcmp(key, "TEST03") == 0)
		{
			format = cJSON_GetObjectItem(json, "TEST03");
			strcpy(Test3.name, key);
			fill_socket_profile(&Test3, format);

			printf("main(9) parse result: %s, %s, %d\n", Test3.name, Test3.ip, Test3.port);

			
			pthread_create(&(Test3.handler),NULL,(void*)tcpSocketThread, &Test3);
		}
	}
	
	printf("main(10) start connect to plc\n");

	tcpSocketThread(&PlcInfo);

	printf("main(11) plc thread exit.\n");
	//tcpSocketThread(&MacMini1);
	//tcpSocketThread(&MacMini2);
	//tcpSocketThread(&MacMini3);
	RfidInfo.exitflag = 1;
	PlcInfo.exitflag = 1;
	PlcMac1Info.exitflag = 1;
	PlcMac2Info.exitflag = 1;
	PlcMac3Info.exitflag = 1;
	MacMini1.exitflag = 1;
	MacMini2.exitflag = 1;
	MacMini3.exitflag = 1;
	Test1.exitflag = 1;
	Test2.exitflag = 1;
	Test3.exitflag = 1;

	pthread_spin_destroy(&(RfidInfo.sendqueue.lock));
	pthread_spin_destroy(&(PlcInfo.sendqueue.lock));
	pthread_spin_destroy(&(PlcMac1Info.sendqueue.lock));
	pthread_spin_destroy(&(PlcMac2Info.sendqueue.lock));
	pthread_spin_destroy(&(PlcMac3Info.sendqueue.lock));
	pthread_spin_destroy(&(MacMini1.sendqueue.lock));
	pthread_spin_destroy(&(MacMini2.sendqueue.lock));
	pthread_spin_destroy(&(MacMini3.sendqueue.lock));
	pthread_spin_destroy(&(Test1.sendqueue.lock));
	pthread_spin_destroy(&(Test2.sendqueue.lock));
	pthread_spin_destroy(&(Test3.sendqueue.lock));

	printf("main(12) free lock.\n");
	if(content != NULL)
	{
		free(content);
	}

	printf("main(15) app exit\n");

	return 0;
}
