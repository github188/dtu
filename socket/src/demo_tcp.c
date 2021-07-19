#include <stdio.h>
#include "global_types.h"
#include "oc_net.h"
#include "sockets.h"
#include "dtu_common.h"
#include "oc_uart.h"

static int sock_fd;
static struct sockaddr_in servaddr;
static OSTaskRef tcpWorkerRef;
static OSTaskRef tcpMonitorRef;
static BOOL bWorkerRun = FALSE;
static BOOL bMonitorRun = FALSE;
extern dtu_config_t g_dtu_config;

void dtu_tcp_send(char *message)
{
	int bytes = send(sock_fd, message, strlen(message), 0);
	printf("bytes = %d", bytes);
}

void tcp_worker_thread(void)
{
	OC_UART_LOG_Printf("[%s] tcp Worker Start!\n", __func__);
	int bytes =0;
	int res=-1;
	char recvbuf[64]={0};
	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(g_dtu_config.currentsocket.tcpport);  // server port
    servaddr.sin_addr.s_addr = inet_addr(g_dtu_config.currentsocket.tcpaddress);  // server ip
	while (TRUE)
	{
		int netstatus = OC_GetNetStatus();
		if(netstatus == 1) { 
			int ret = connect(sock_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
			OC_UART_LOG_Printf("[%s] Connection = %d!\n", __func__, ret);
			if(ret == 0){
				break;
			}
		}
		OSATaskSleep(200); 
	}
	
    OC_UART_LOG_Printf("[%s] connect Success\n", __func__);
	while (bWorkerRun)
	{
		bytes = recv(sock_fd, recvbuf, sizeof(recvbuf), 0);
		if(bytes <= 0) {
			OC_UART_LOG_Printf("[%s]: socket disconnect\n", __func__);
			bWorkerRun = FALSE;
			break;
		}
		
		OC_UART_LOG_Printf("[%s] recvbuf:%s,bytes:%d", __func__,recvbuf,bytes);
		received_from_server(recvbuf, bytes, TRANS_TCP);
	}
}

void tcp_monitor_thread(void)
{
	OC_UART_LOG_Printf("[%s] tcp Monitor Start!\n", __func__);
	void *TcpWorkTaskStack;
	TcpWorkTaskStack=malloc(4096);
	if(TcpWorkTaskStack == NULL)
	{
		return;
	}
	bWorkerRun = TRUE;
	if(OSATaskCreate(&tcpWorkerRef,
	                 TcpWorkTaskStack,
	                 4096,80,(char*)"tcpWorkerTask",
	                 tcp_worker_thread, NULL) != 0)
	{
		return;
	}
	
	while (bMonitorRun)
	{
		if(bWorkerRun == FALSE){
			int netstatus = OC_GetNetStatus();
			if(netstatus == 1) { // 网络恢复后再重新连接
				OSATaskDelete(tcpWorkerRef); 
				bWorkerRun = TRUE;
				if(OSATaskCreate(&tcpWorkerRef,
								TcpWorkTaskStack,
								4096,80,(char*)"tcpWorkerTask",
								tcp_worker_thread, NULL) != 0)
				{
					return;
				}
			}
		}
		OSATaskSleep(2000); //每10s查询一次TCP状态
	}
}

void customer_app_tcp_start(void)
{
	void *TcpMonitorTaskStack;
	TcpMonitorTaskStack=malloc(4096);
	if(TcpMonitorTaskStack == NULL)
	{
		return;
	}
	bMonitorRun = TRUE;
	if(OSATaskCreate(&tcpMonitorRef,
	                 TcpMonitorTaskStack,
	                 4096,80,(char*)"tcpMonitorTask",
	                 tcp_monitor_thread, NULL) != 0)
	{
		return;
	}
}