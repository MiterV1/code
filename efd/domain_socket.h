#ifndef __DOMAIN_SOCKET_H__
#define __DOMAIN_SOCKET_H__

extern ssize_t send_fd(int sock, int fd);
extern ssize_t recv_fd(int sock, int *fd);
extern int create_domain_socket_server(char *path);
extern int connect_domain_socket_server(char *path);
#endif