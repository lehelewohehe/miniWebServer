/* ************************************************************************
 *       Filename:  http_server.h
 *    Description:  
 *        Version:  1.0
 *        Created:  2019年03月22日 18时01分17秒
 *       Revision:  none
 *       Compiler:  gcc
 *         Author:  YOUR NAME (), 
 *        Company:  
 * ************************************************************************/

#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

void epoll_run(int);
int init_listen_fd(int,int);
void do_accept(int,int);
void do_read(void*,int);
int get_line(int,char*,int);
void myperror(char*);
void decode_str(char*,char*);
void encode_str(char*,int,const char*);
const char* get_file_type(const char*);
void http_request(const char *,int);
void send_file(const char*,int);
void send_dir(const char*,int);
void showerr(int);
void send_head_respond(int,int,const char*,const char*,long);
void disconnected(int,void *);
int hexit(char);

#endif


