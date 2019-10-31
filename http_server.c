/* ************************************************************************
 *       Filename:  http_server.c
 *    Description:  
 *        Version:  1.0
 *        Created:  2019年03月22日 16时28分48秒
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/

#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<string.h>
#include"http_server.h"
#include<dirent.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<ctype.h>

typedef struct infonode
{
    int fd;
	struct acokaddr_in addr_c;
}Infonode;

void myperror(char *src)
{
	perror(src);
	exit(-1);
}


void epoll_run(int port)
{
	//创建epoll树根节点
	int epfd=epoll_create(2000);
	if(epfd==-1)
	{
		myperror("epoll_create err");
	}
	//创建监听描述符
	//监听绑定
	//将监听描述符添加到epoll树上
	int sfd=init_listen_fd(port,epfd);
	//定义客户端事件描述数组以及数组最大值
	struct epoll_event all[2000];
	int max=sizeof(all)/sizeof(all[0]);

    while(1)
	{
        //内核帮助用户完成io转接或者说是检测客户端事件
		//该函数会返回事件数量以及具体哪些客户端有事件产生
		int num=epoll_wait(epfd,all,max,-1);
		if(num==-1)
		{
			myperror("epoll_wait err");
		}
		//事件处理
		int i=0;
		for(;i<num;i++)
		{
			//默认只处理读事件
			if(!(all[i].events&EPOLLIN))
			{
				continue;
			}
			int fd=(Infonode*)(all[i].data.ptr)->fd;
			if(fd==sfd)
			{
				//处理连接请求
				do_accept(sfd,epfd);
			}
			else
			{
				//客户端数据处理
				do_read(all[i].data.ptr.epfd);
			}
		}

	}

}

void do_read(void *zptr,int epfd)
{
	//从ptr中获取通讯描述符和客户端信息
	Infonode* ptr=(Infonode*)zptr;
	int fd=ptr->fd;
	char line[1024]={0};
	int len=get_line(fd,line,sizeof(line));
	if(len==0)
	{
		disconnected(epfd,zptr);
	}
	else if(len==-1)
	{
		printf("get_line err\n");
		exit(-1);
	}
	else
	{
		printf("请求行:%s",line);
		char buf[1024]={0};
		printf("=============请求头=============\n");
		while((len=get_line(fd,buf,sizeof(buf)))>0)
		{
            printf("%s",buf);
		}
	}
	//判断协议类型
	//加入是get类型
	if(strncasecmp("GET",line,3)==0)
	{
		//请求处理函数
		http_request(line,fd);
	}
	else
	{
        //默认暂时不处理其他请求类型
	}	
}

void http_request(const char *request,int cfd)
{
    //使用正则表达式拆分请求行
	char method[12]={0};
	char path[1024]={0};
	char protocal[12]={0};
	sscanf(request,"%[^ ] %[^ ] %[^ ]",method,path,protocal);
	printf("method=%s,path=%s,protocal=%s\n",method,path,protocal);

	//因为浏览器不支持中文，在发送中文请求时将自动进行16进制编码
	//在拿到文件名字之后应对其解码
	decode_str(path,path);
	//去除文件名的前面的/
	char *file=path+1;
	//如果用户在访问服务器时不输入指定内容
	//则默认访问资源目录根目录
	if(strcmp("/",path)==0)
	{
		file="./";
	}
	//获取文件属性的相关信息
	struct stat sb;
	int ret=stat(file,&sb);
	if(ret==-1)
	{
		myperror("stat err");
	}
	//判断文件是目录还是普通文件
	if(S_ISDIR(sb.st_mode))
	{
		//目录
		//发送消息头
		send_head_respond(cfd,200,"ok",get_file_type(".html"),-1)
		//读目录
		send_dir(file,cfd);
	}
	if(S_ISREG(sb.st_mode))
	{
		//文件
		//发送消息头
		send_head_respond(cfd,200,"ok",get_file_type(file),sb.st_size);
		//读文件
		send_file(file,cfd);
	}
}

void send_file(const char*filename,int cfd)
{
	//打开文件
	int fd=open(filename,O_RDONLY);
	if(fd==-1)
	{
		//文件存在或者出错，返回一个错误页面
		showerr(cfd);
		return;
	}
	//循环读数据
	int len=0;
	while((len=read(fd,buf,sizeof(buf)))>0)
	{
		write(cfd,buf,len);
	}
	if(len==-1)
	{
		myperror("read file err");
	}
	close(fd);
}

void send_dir(const char *dirname,int cfd)
{
	//读目录的时候是将目录内容组织成一个网页发送给客户端
	//拼接要发送的网页
	char buf[4096]={0};
	sprintf(buf,"<html><head><title>%s</title></head><body>",dirname);
	sprintf(buf,"<h1>%s</h1><table>",dirname);
	//读目录获取目录中所有文件的名字
	struct dirent **dirs;
	//返回值是目录中文件的数量,alphasort是一个排序回调
	//系统提供的
	int num=scandir(dirname,&dirs,NULL,alphasort);
	if(num==-1)
	{
		//读取出错当回出错页面
		showerr(cfd);
	}
	int i=0;
	//遍历拼接目录中的文件或者目录
	for(i;i<num;i++)
	{
		char *name=dirs->d_name;
		//获取编码后的字符串
		char encode[1024]={0};
		encode_str(encode,sizeof(encode),name);
		char path[1024]={0};
		//获取文件的属性
		//拼接完整的文件路径
        sprintf(path,"%s/%s",dirname,name);
		struct stat sb;
		stat(path,&sb);
        //判断是文件还是目录
		if(S_ISREG(sb.st_mode))
		{
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s\">%s</td><td>%ld</td></tr>",encode,name,sb.st_size);
		}
		else if(S_ISDIR(sb.st_mode))
		{
            sprintf(buf+strlen(buf),"<tr><td><a href=\"%s/\">%s</td><td>%ld</td></tr>",encode,name,sb.st_size);
		}
	}
	sprintf(buf+strlen(buf),"</table></body></html>");
	//将拼接好的数据发送给客户端
	send(cfd,buf,strlen(buf),0);
}

void showerr(int cfd)
{
	send_head_respond(cfd,200,"ok","imag/jpeg",-1);
	//读文件
	send_file("404.jpg");
}

void send_head_respond(int cfd,int no,const char*desp,const char *type,long size)
{
	char buf[1024]={0};
	//拼接响应消息的状态行
	sprintf(buf,"HTTP/1.1 %d %s\r\n",no,desp);
	//拼接消息报头
	sprintf(buf+strlen(buf),"Content-Type:%s\r\n",type);
	sprintf(buf+strlen(buf),"Content-Length:%ld\r\n",size);
	send(cfd,buf,strlen(buf),0);
	//发送空行
	send(cfd,"\r\n",2,0);
}

void disconnected(int epfd,void *zptr)
{
	Infonode *ptr=(Infonode*)zptr;
	char ip[64]={0};
	printf("客户端ip=%s,port=%d,断开连接\n",inet_ntop(AF_INET,&(ptr->addr_c.sin_addr.s_addr),ip,sizeof(ip))
				,ntohs(ptr->addr_c.sin_port));
	//关闭文件描述符和释放资源
	int ret=epoll_ctl(epfd,EPOLL_CTL_DEL,fd,NULL);
	if(ret==-1)
	{
		myperror("epoll_ctl err");
	}
	close(ptr->fd);
	free(ptr);
}


void do_accept(int sfd,int epfd)
{
	//初始化客户端信息变量
	struct sockaddr_in addr_c;
	memset(&addr_c,0,sizeof(addr_c));
	socklen_t len_c=sizeof(addr_c);
	//接受连接请求
    int cfd=accept(sfd,(struct sockaddr*)&addr_c,&len_c);
	if(cfd==-1)
	{
		myperror("accept err");
	}
	char ip[64]={0};
	printf("客户端ip=%s,port=%d,断开连接\n",inet_ntop(AF_INET,&addr_c.sin_addr.s_addr,ip,sizeof(ip))
				,ntohs(addr_c.sin_port));
	//将通信描述符设置为非阻塞
	int flag=fcntl(cfd,F_GETFL);
	flag=flag|O_NONBLOCK;
	fcntl(cfd,F_SETFL,flag);
	//初始化描述cfd描述符的信息,设置边沿模式
	Infonode *ptr=malloc(sizeof(Infonode));
	memset(ptr,0,sizeof(Infonode));
	ptr->fd=cfd;
	ptr->addr_c=addr_c;
	struct epoll_event ev;
	ev.data.ptr=ptr;
	ev.events=EPOLLIN|EPOLLET;
	//将新加入的通信描述符上树
	int ret=epoll_ctl(epfd,EPOLL_CTL_ADD,cfd,&ev);
	if(ret==-1)
	{
		myperror("epoll_ctl cfd err");
	}
}

int init_listen_fd(int port,int epfd)
{
	//创建socket监听套接字
	int sfd=socket(AF_INET,SOCK_STREAM,0);
	if(sfd==-1)
	{
		myperror("socket sfd err");
	}
	//设置端口复用
	int flag=1;
	setsockopt(sfd,SOL_STREAM,SO_REUSEADDR,&flag,sizeof(flag));
	//初始化端口和ip
	struct sockaddr_in addr_s;
	addr_s.sin_family=AF_INET;
	addr_s.sin_port=htons(port);
	addr_s.sin_addr.s_addr=htonl(INADDR_ANY);
	//绑定本地IP和端口
	int ret=bind(sfd,(struct sockaddr*)&addr_s,sizeof(addr_s));
	if(ret==-1)
	{
		myperror("bind err");
	}
	//注册监听套接字
	ret=listen(sfd,64);
	if(ret==-1)
	{
		myperror("listen err");
	}
	//初始化描述sfd的信息
	Infonode *ptr=malloc(sizeof(Infonode));
	memset(ptr,0,sizeof(Infonode));
	ptr->fd=sfd;
	struct epoll_events ev;
	ev.data.ptr=ptr;
	ev.events=EPOLLIN;
	//将监听套接字加入到树上
	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,sfd,&ev);
	if(ret==-1)
	{
		myperror("epoll_ctl sfd err");
	}
	return sfd;
}

// 解析http请求消息的每一行内容
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                {
                    recv(sock, &c, 1, 0);
                }
                else
                {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

// 16进制数转化为10进制
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/*
 *  这里的内容是处理%20之类的东西！是"解码"过程。
 *  %20 URL编码中的‘ ’(space)
 *  %21 '!' %22 '"' %23 '#' %24 '$'
 *  %25 '%' %26 '&' %27 ''' %28 '('......
 *  相关知识html中的‘ ’(space)是&nbsp
 */
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;

    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) 
    {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) 
        {
            *to = *from;
            ++to;
            ++tolen;
        } 
        else 
        {
            sprintf(to, "%%%02x", (int) *from & 0xff);
            to += 3;
            tolen += 3;
        }

    }
    *to = '\0';
}


void decode_str(char *to, char *from)
{
    for ( ; *from != '\0'; ++to, ++from  ) 
    {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) 
        { 

            *to = hexit(from[1])*16 + hexit(from[2]);

            from += 2;                      
        } 
        else
        {
            *to = *from;

        }

    }
    *to = '\0';

}

// 通过文件名获取文件的类型
const char *get_file_type(const char *name)
{
    char* dot;

    // 自右向左查找‘.’字符, 如不存在返回NULL
    dot = strrchr(name, '.');   
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp( dot, ".wav" ) == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}
