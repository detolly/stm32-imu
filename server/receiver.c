/*
 * Minimal telemetry receiver.
 *
 * Build: cc -O2 -Wall -Wextra -o receiver receiver.c
 * Run:   ./receiver [port]     (default port: 5000)
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 5000
#define MAX_CLIENTS  16
#define RECORD_SIZE  8U    /* float roll followed by float pitch */
#define BUFFER_SIZE  4096U

typedef struct {
    int fd;
    char ip[INET_ADDRSTRLEN];
    unsigned port;
    uint8_t buffer[BUFFER_SIZE];
    size_t used;
} client_t;

static uint32_t read_u32_le(const uint8_t *data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static float read_float_le(const uint8_t *data)
{
    uint32_t bits = read_u32_le(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void disconnect_client(client_t *client)
{
    printf("Client disconnected: %s:%u\n", client->ip, client->port);
    close(client->fd);
    client->fd = -1;
    client->used = 0U;
}

static void receive_client(client_t *client)
{
    ssize_t received = recv(client->fd, client->buffer + client->used,
                            sizeof(client->buffer) - client->used, 0);
    size_t offset = 0U;

    if (received == 0) {
        disconnect_client(client);
        return;
    }
    if (received < 0) {
        if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return;
        }
        perror("recv");
        disconnect_client(client);
        return;
    }

    client->used += (size_t)received;

    while ((client->used - offset) >= RECORD_SIZE) {
        float roll = read_float_le(client->buffer + offset);
        float pitch = read_float_le(client->buffer + offset + 4U);

        printf("%s:%u roll=%8.2f  pitch=%8.2f\n",
               client->ip, client->port, roll, pitch);
        offset += RECORD_SIZE;
    }

    if (offset != 0U) {
        memmove(client->buffer, client->buffer + offset,
                client->used - offset);
        client->used -= offset;
    }
}

static void accept_clients(int listen_fd, client_t *clients)
{
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    int client_fd = accept(listen_fd, (struct sockaddr *)&address, &length);
    int slot;

    if (client_fd < 0) {
        if ((errno != EINTR) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
            perror("accept");
        }
        return;
    }

    for (slot = 0; slot < MAX_CLIENTS; ++slot) {
        if (clients[slot].fd < 0) {
            break;
        }
    }

    if (slot == MAX_CLIENTS) {
        printf("Rejecting client: too many connections\n");
        close(client_fd);
        return;
    }

    clients[slot].fd = client_fd;
    clients[slot].port = (unsigned)ntohs(address.sin_port);
    clients[slot].used = 0U;

    if (inet_ntop(AF_INET, &address.sin_addr, clients[slot].ip,
                  sizeof(clients[slot].ip)) == NULL) {
        strcpy(clients[slot].ip, "?");
    }

    printf("Client connected: %s:%u\n",
           clients[slot].ip, clients[slot].port);
}

int main(int argc, char **argv)
{
    long port = DEFAULT_PORT;
    int listen_fd;
    int reuse = 1;
    struct sockaddr_in address;
    client_t clients[MAX_CLIENTS];
    int slot;

    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc > 1) {
        char *end = NULL;

        port = strtol(argv[1], &end, 10);
        if ((*end != '\0') || (port < 1) || (port > 65535)) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof(reuse));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);

    if ((bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) ||
        (listen(listen_fd, 8) < 0)) {
        perror("bind/listen");
        close(listen_fd);
        return 1;
    }

    for (slot = 0; slot < MAX_CLIENTS; ++slot) {
        clients[slot].fd = -1;
    }

    printf("Listening on port %ld\n", port);

    for (;;) {
        struct pollfd fds[MAX_CLIENTS + 1];
        int client_slot[MAX_CLIENTS + 1];
        nfds_t count = 1U;
        int ready;

        fds[0].fd = listen_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        client_slot[0] = -1;

        for (slot = 0; slot < MAX_CLIENTS; ++slot) {
            if (clients[slot].fd >= 0) {
                fds[count].fd = clients[slot].fd;
                fds[count].events = POLLIN;
                fds[count].revents = 0;
                client_slot[count] = slot;
                ++count;
            }
        }

        ready = poll(fds, count, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if ((fds[0].revents & POLLIN) != 0) {
            accept_clients(listen_fd, clients);
        }

        for (nfds_t i = 1U; i < count; ++i) {
            if ((fds[i].revents &
                 (POLLIN | POLLHUP | POLLERR | POLLNVAL)) != 0) {
                receive_client(&clients[client_slot[i]]);
            }
        }
    }

    close(listen_fd);
    return 0;
}
