#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#define CLIENT_PATH "tpf_unix_sock.client"

ssize_t send_fd(int sock, int fd)
{
    char dummy;
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    iov.iov_base = &dummy;
    iov.iov_len  = sizeof(dummy);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_level = SOL_SOCKET;
    *((int *)CMSG_DATA(cmsg)) = fd;

    return (sendmsg(sock, &msg, 0));
}

ssize_t recv_fd(int sock, int *fd)
{
    ssize_t n;
    char dummy;
    struct iovec iov;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    iov.iov_base = &dummy;
    iov.iov_len  = sizeof(dummy);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    n = recvmsg(sock, &msg, 0);
    if (n <= 0) {
        perror("recvmsg");
        return (n);
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
        if (cmsg->cmsg_level != SOL_SOCKET) {
            printf("invalid cmsg_level %d\n", cmsg->cmsg_level);
            return -1;
        }
        if (cmsg->cmsg_type != SCM_RIGHTS) {
            printf("invalid cmsg_type %d\n", cmsg->cmsg_type);
            return -2;
        }
        *fd = *((int *)CMSG_DATA(cmsg));
    } else {
        *fd = -1; /* descriptor was not passed */
    }

    return (n);
}

int create_domain_socket_server(char *path)
{
    int sfd, ret;
    int backlog = 5;
    struct sockaddr_un address;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
        printf("SOCKET ERROR: %d\n", sfd);
        return -1;
    }

    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path);

    unlink(path);
    ret = bind(sfd, (struct sockaddr *)&address, sizeof(address));
    if (ret < 0) {
        printf("BIND ERROR: %d\n", ret);
        close(sfd);
        return -1;
    }

    ret = listen(sfd, backlog);
    if (ret < 0) { 
        printf("LISTEN ERROR: %d\n", ret);
        close(sfd);
        return -1;
    }

    return sfd;
}

int connect_domain_socket_server(char *path)
{
    static int i;
    int sfd, ret;
    char buffer[BUFSIZ];
    struct sockaddr_un address;

    sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
        printf("SOCKET ERROR: %d\n", sfd);
        return -1;
    }

    sprintf(buffer, "%s_%d", CLIENT_PATH, i++);

    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, buffer, strlen(buffer) + 1);

    unlink(buffer);
    ret = bind(sfd, (struct sockaddr *)&address, sizeof(address));
    if (ret < 0) {
        printf("BIND ERROR: %d\n", ret);
        close(sfd);
        return -1;
    }

    strncpy(address.sun_path, path, strlen(path) + 1);
    ret = connect(sfd, (struct sockaddr *)&address, sizeof(address));
    if (ret < 0) { 
        printf("CONNECT ERROR: %d\n", ret);
        close(sfd);
        return -1;
    }

    return sfd;
}
