/*
 ============================================================================
 Name        : efd.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include <sys/socket.h>
#include <sys/un.h>

#include "efd_sema.h"
#include "domain_socket.h"

#define SERVER_PATH "tpf_unix_sock.server"

int main(void) {
    int sfd, cfd, len;
    struct sockaddr_un address;
    int server_efd, client_efd;

    server_efd = semaphore_new();
    sfd = create_domain_socket_server(SERVER_PATH);
    while (1) {
        cfd = accept(sfd, (struct sockaddr *)&address, &len);
        if (cfd < 0) {
            continue;
        }        

        recv_fd(cfd, &client_efd);
        printf("recving client efd=%d\n", client_efd);
        sleep(5);

       	send_fd(cfd, server_efd);
        printf("sending server efd=%d\n", server_efd);

       	sleep(5);
       	printf("starting post!!!\n");
       	semaphore_post(client_efd);

        sleep(5);
        printf("starting post!!!\n");
        semaphore_post(client_efd);

        printf("server wait post!!!\n");
        semaphore_wait(server_efd);
        printf("server wait from post!!!\n");
        printf("server return !!!\n");

        printf("server wait post!!!\n");
        semaphore_wait(server_efd);
        printf("server wait from post!!!\n");
        printf("server return !!!\n");
    }
}

