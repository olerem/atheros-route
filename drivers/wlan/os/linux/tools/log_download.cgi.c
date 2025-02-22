#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>
#include<unistd.h>
#include "filelist.h"

#define MAX_FILE_LEN  (1024*30)
#define DOWNLOAD_FILE_PATH	"/tmp/"
#define DOWNLOAD_FILE_NAME	"systemLog"


static FILE *errOut;
    
int main()
{
	errOut = fopen("/dev/ttyS0","w");
	FILE *fp, *fpp;
	char filebuf[MAX_FILE_LEN];
	char cmd[512];
	struct stat sb;

	//added by yhl 20150107
	system("echo *******check_pppoe_ps********* >> /tmp/systemLog");
	system("ps  >>  /tmp/systemLog");
	system("echo *******check_pppoe_cfg******** >> /tmp/systemLog");
	system("cfg -e | grep PPPOE  >> /tmp/systemLog");
	system("echo ******check_pppoe_chap******* >> /tmp/systemLog");
	system("cat /etc/ppp/chap-secrets >> /tmp/systemLog");
	
	system("echo *******check_pppoe_ifg******* >> /tmp/systemLog");
	system("ifconfig >> /tmp/systemLog");
	
	system("echo *******check_pppoe_route***** >> /tmp/systemLog");
	system("route -n >> /tmp/systemLog");
	
	system("echo *******check_pppoe_try********* >> /tmp/systemLog");
	system("pppoe-try >> /tmp/systemLog");

	system("echo *******check_pppoe_tcpdps********** >> /tmp/systemLog");
//	system("tcpdumps -c 30 >> /tmp/systemLog");

	sprintf(cmd, "%s%s", DOWNLOAD_FILE_PATH, DOWNLOAD_FILE_NAME);
	stat(cmd, &sb); //取待下载文件的大小
	
	//输出HTTP头信息，输出附加下载文件、文件长度以及内容类型
	printf("Content-Disposition:attachment;filename=%s", DOWNLOAD_FILE_NAME);
	printf("\r\n"); 
	printf("Content-Length:%d", sb.st_size);
	printf("\r\n");
//	printf("Content-Type:application/octet-stream %c%c", 13,10); (ascii:"13--\r", "10--\n") 与下一行等同
	printf("Content-Type:application/octet-stream\r\n");
	printf("\r\n");
	sprintf(cmd, "%s%s", DOWNLOAD_FILE_PATH, DOWNLOAD_FILE_NAME);
	
	if(fp=fopen(cmd, "r+b")){  
	//成功打开文件，读取文件内容
		do{
			int rs = fread(filebuf, 1, sizeof(filebuf), fp);
			
			fwrite(filebuf, rs, 1, stdout);
		}while(!feof(fp));
		fclose(fp);	
	}

	return 1;
}

