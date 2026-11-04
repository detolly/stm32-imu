/*
 * Minimal telemetry receiver.
 *
 * Build: cc -O2 -Wall -Wextra -o receiver receiver.c
 * Run:   ./receiver [port]     (default port: 5000)
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 5000
#define RECORD_SIZE  8U    /* float roll followed by float pitch */
#define BUFFER_SIZE  4096U

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

int main(int argc, char **argv)
{
    long port = DEFAULT_PORT;
    int listen_fd;
    int reuse = 1;
    struct sockaddr_in address;

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

    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("Listening on port %ld\n", port);

    for (;;) {
        struct sockaddr_in client;
        socklen_t client_length = sizeof(client);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client,
                               &client_length);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }

        printf("Client connected: %s:%u\n",
               inet_ntoa(client.sin_addr),
               (unsigned)ntohs(client.sin_port));

        {
            uint8_t buffer[BUFFER_SIZE + RECORD_SIZE];
            size_t used = 0U;

            for (;;) {
                ssize_t received = recv(client_fd, buffer + used,
                                        sizeof(buffer) - used, 0);
                size_t offset = 0U;

                if (received == 0) {
                    break;
                }
                if (received < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    perror("recv");
                    break;
                }

                used += (size_t)received;

                while ((used - offset) >= RECORD_SIZE) {
                    float roll = read_float_le(buffer + offset);
                    float pitch = read_float_le(buffer + offset + 4U);

                    printf("roll=%8.2f  pitch=%8.2f\n", roll, pitch);
                    offset += RECORD_SIZE;
                }

                if (offset != 0U) {
                    memmove(buffer, buffer + offset, used - offset);
                    used -= offset;
                }
            }
        }

        close(client_fd);
        printf("Client disconnected\n");
    }

    close(listen_fd);
    return 0;
}
