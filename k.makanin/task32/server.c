#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <time.h>
#include <fcntl.h>

#define SOCKET_PATH "./socket"
#define MAX_CLIENTS 10
#define SEND_INTERVAL_NS 1000000 

void sigCatch(int sig) {
    unlink(SOCKET_PATH);
    _exit(1);
}

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    char buffer[BUFSIZ];
    int socketFd;
    if ((socketFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("Socket failed");
        exit(-1);
    }

    setNonBlocking(socketFd);

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_UNIX;
    strncpy(serverAddr.sun_path, SOCKET_PATH, sizeof(serverAddr.sun_path) - 1);

    if (bind(socketFd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Bind failed");
        exit(-1);
    }
    signal(SIGINT, sigCatch);
    signal(SIGQUIT, sigCatch);

    if (listen(socketFd, MAX_CLIENTS) == -1) {
        unlink(SOCKET_PATH);
        perror("Listen error");
        exit(-1);
    }

    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        unlink(SOCKET_PATH);
        perror("Epoll create failed");
        exit(-1);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = socketFd;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, socketFd, &event) == -1) {
        unlink(SOCKET_PATH);
        perror("Epoll ctl failed");
        exit(-1);
    }

    struct epoll_event events[MAX_CLIENTS + 1];
    int clientFds[MAX_CLIENTS];
    int clientCount = 0;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long nextSendTime = ts.tv_nsec + SEND_INTERVAL_NS;

    while (1) {
        int nfds = epoll_wait(epollFd, events, MAX_CLIENTS + 1, -1);
        if (nfds == -1) {
            unlink(SOCKET_PATH);
            perror("Epoll wait failed");
            exit(-1);
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == socketFd) {
                int clientFd = accept(socketFd, NULL, NULL);
                if (clientFd == -1) {
                    perror("Accept failed");
                    unlink(SOCKET_PATH);
                    exit(-1);
                }

                setNonBlocking(clientFd);

                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clientFd;
                if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
                    perror("Epoll ctl add client failed");
                    close(clientFd);
                } else {
                    clientFds[clientCount++] = clientFd;
                }
            } else {
                int clientFd = events[i].data.fd;
                size_t bytesRead = read(clientFd, buffer, BUFSIZ);
                if (bytesRead <= 0) {
                    close(clientFd);
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, NULL);
                    for (int j = 0; j < clientCount; j++) {
                        if (clientFds[j] == clientFd) {
                            clientFds[j] = clientFds[--clientCount];
                            break;
                        }
                    }
                } else {
                    for (size_t j = 0; j < bytesRead; j++) {
                        putc(toupper(buffer[j]), stdout);
                    }
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        if (ts.tv_nsec >= nextSendTime) {
            for (int i = 0; i < clientCount; i++) {
                write(clientFds[i], "a", 1);
            }
            nextSendTime = ts.tv_nsec + SEND_INTERVAL_NS;
        }
    }
}