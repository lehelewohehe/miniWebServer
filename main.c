/* ************************************************************************
 *       Filename:  main.c
 *    Description:  
 *        Version:  1.0
 *        Created:  2019年03月22日 16时23分57秒
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"http_server.h"

int main(int argc,char *argv[])
{
	if(argc!=3)
	{
		printf("./a.out port dir\n");
		exit(-1);
	}

	int port=atoi(argv[1]);
	//切换进程到资源目录根目录
	int ret=chdir(argv[2]);
	if(ret==-1)
	{
		perror("chdir err\n");
		exit(-1);
	}
	//启动epoll模型
	epoll_run(port);
	return 0;
}


