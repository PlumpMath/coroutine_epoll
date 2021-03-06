#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "pt_server.h"

#define MAX_CONNECTION  (1024*128)

pt_server *create_pt_server(const char *ip, int port, void *(*handle)(void *))
{
    struct sockaddr_in server_address;
    in_addr_t server_ip;
    int optval = 0;
    pt_server *server = calloc(1, sizeof(pt_server));

    if (!ip) {
        server_ip = htonl(INADDR_ANY);
    } else if ((server_ip = inet_addr(ip)) == INADDR_NONE) {
        return NULL;
    }

    if (handle == NULL) {
        return NULL;
    }

    if ((server->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }

    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        return NULL;
    }

    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = server_ip;
    server_address.sin_port = htons(port);

    if (bind(server->socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        return NULL;
    }

    server->handle = handle;
    return server;
}

void free_pt_server(pt_server *server)
{
    close(server->socket);
    free(server);
}

void run_pt_server(pt_server *server)
{
    struct epoll_event ev;
    struct epoll_event *events;
    struct sockaddr_in remote;
    socklen_t remote_len;
    int epfd;
    int nfds;
    assert(listen(server->socket, MAX_CONNECTION) == 0);
    assert((epfd = epoll_create(128)) >= 0);
    ev.events = EPOLLIN;
    ev.data.fd = server->socket;
    assert(epoll_ctl(epfd, EPOLL_CTL_ADD, server->socket, &ev) >= 0);

    remote_len = sizeof(remote);
    events = calloc(MAX_CONNECTION, sizeof(struct epoll_event));
    while (1) {
        nfds = epoll_wait(epfd, events, MAX_CONNECTION, -1);
        int i;
        for (i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server->socket) {
                int *client_socket = malloc(sizeof(int));
                *client_socket = accept(server->socket, (struct sockaddr *) &remote, &remote_len);

                if (*client_socket < 0) {
                    printf("accept client connection fail!\n");
                    free(client_socket);
                    continue;
                }
                pthread_t tid;
                pthread_create(&tid, NULL, server->handle, (void *) client_socket);
            }
        }
    }
    free(events);
    close(epfd);
}
