#ifndef _INTERNETWORK_COMMUNICATION_H_
#define _INTERNETWORK_COMMUNICATION_H_

#include "common_tools.h"
#if CTSI_SECURITY_SCHEME == 1
#include "unionpay_message.h"
#else 
#include "safe_strategy.h"
#endif
#include "communication_stc.h"

// 结构体定义
struct class_internetwork_communication
{
    int (* make_local_socket_server_link)(const char *socket_name);
    int (* make_server_link)(const char *port);
    int (* accept_client_connection)(const int server_fd);
    int (* make_local_socket_client_link)(const char *socket_name); 
    int (* make_client_link)(const char *ip, const char *port); 
    int (* msg_pack)(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_pack2)(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_unpack)(struct s_data_list *a_data_list, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_unpack2)(char *buf, unsigned short buf_len, struct s_dial_back_respond *a_dial_back_respond);
    int (* msg_send)(int fd, struct s_data_list *a_data_list);
    int (* msg_recv)(int fd, struct s_data_list *a_data_list);
    int (* msg_recv2)(int fd, char *buf, unsigned short buf_len);
    #if CTSI_SECURITY_SCHEME == 1
    int (* unpack_pack_send_data)(struct s_data_list *a_data_list, char a_pack_type, void *a_par);
    #endif
    //int (* send)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
    //int (* recv)(unsigned int fd, unsigned char *data, struct s_data_list *a_data_list, unsigned int data_len, struct timeval *tv);
};
extern struct class_internetwork_communication internetwork_communication;
#endif
