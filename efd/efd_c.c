/*
 ============================================================================
 Name        : efd.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "efd_sema.h"
#include "domain_socket.h"

#define SERVER_PATH "tpf_unix_sock.server"

int main(void) {
    int sfd, len;
    int server_efd, client_efd;

    client_efd = semaphore_new();
    sfd = connect_domain_socket_server(SERVER_PATH);
    while (1) {
        sleep(5);
       	send_fd(sfd, client_efd);
        printf("sending client efd=%d\n", client_efd);

        recv_fd(sfd, &server_efd);
        printf("recving server efd=%d\n", server_efd);

        printf("wait post!!!\n");
        semaphore_wait(client_efd);
        printf("wait from post!!!\n");
        printf("return !!!\n");

        printf("wait post!!!\n");
        semaphore_wait(client_efd);
        printf("wait from post!!!\n");
        printf("return !!!\n");

        sleep(5);
        printf("starting server post!!!\n");
        semaphore_post(server_efd);

        sleep(5);
        printf("starting server post!!!\n");
        semaphore_post(server_efd);

        return 0;
    }
}

