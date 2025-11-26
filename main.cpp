#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

constexpr int PORT = 8009;

volatile sig_atomic_t wasSigHup = 0;
int listen_fd = -1;
int client_fd = -1;

void sigHupHandler(int) 
{
    wasSigHup = 1;
}

int main() 
{
    struct sigaction sa;
    sigaction(SIGHUP, nullptr, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, nullptr);

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) 
    {
        perror("sigprocmask");
        return EXIT_FAILURE;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) 
    { 
        perror("socket"); 
        return EXIT_FAILURE; 
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) 
    {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, SOMAXCONN) == -1) 
    {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", PORT);

    while (true) 
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_fd, &fds);
        int max_fd = listen_fd;

        if (client_fd != -1) 
        {
            FD_SET(client_fd, &fds);
            if (client_fd > max_fd) max_fd = client_fd;
        }

        int ret = pselect(max_fd + 1, &fds, nullptr, nullptr, nullptr, &origMask);
        if (ret == -1) 
        {
            if (errno == EINTR) 
            {
                if (wasSigHup) 
                {
                    printf("Received SIGHUP\n");
                    wasSigHup = 0;
                }
                continue;
            }
            perror("pselect");
            break;
        }

        if (FD_ISSET(listen_fd, &fds)) 
        {
            struct sockaddr_in cli{};
            socklen_t len = sizeof(cli);
            int fd = accept(listen_fd, (struct sockaddr*)&cli, &len);
            if (fd == -1) 
            {
                perror("accept");
                continue;
            }

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
            printf("New connection from %s:%d\n", ip, ntohs(cli.sin_port));

            if (client_fd == -1) 
            {
                client_fd = fd;
                printf("Connection accepted\n");
            } 
            else 
            {
                printf("Rejecting additional connection from %s:%d\n", ip, ntohs(cli.sin_port));
                close(fd);
            }
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &fds)) 
        {
            char buf[4096];
            ssize_t n = recv(client_fd, buf, sizeof(buf), 0);

            if (n > 0) 
            {
                printf("Received %zd bytes\n", n);
            } 
            else if (n == 0) 
            {
                printf("Client closed connection\n");
                close(client_fd);
                client_fd = -1;
            } 
            else 
            {
                perror("recv");
                close(client_fd);
                client_fd = -1;
            }
        }
    }

    if (listen_fd != -1) 
    {
        close(listen_fd);
    }
    
    if (client_fd != -1) 
    {
        close(client_fd);
    }
    return 0;
}