#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <stdint.h>

void print_help() {
    printf("Использование: ./lab3asdN3252_client [опции] <IP> <Порт> <Данные(Hex)>\n");
    printf("Пример: ./lab3asdN3252_client 127.0.0.1 8080 0x02AABB\n");
    printf("  0x02 - длина данных (2 байта)\n");
    printf("  AABB - сами данные\n");
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "vh")) != -1) {
        if (opt == 'v') {
            printf("Клиент версии 1.0\nСвиржевский А.Д., N3252\nВариант 16\n");
            return 0;
        }
        if (opt == 'h') {
            print_help();
            return 0;
        }
    }

    if (optind + 3 > argc) {
        fprintf(stderr, "Ошибка: недостаточно аргументов\n");
        print_help();
        return EXIT_FAILURE;
    }

    char *ip = argv[optind];
    int port = atoi(argv[optind+1]);
    char *hex = argv[optind+2];

    // Парсинг Hex-строки
    if (strncmp(hex, "0x", 2) == 0 || strncmp(hex, "0X", 2) == 0) {
        hex += 2;
    }

    size_t len = strlen(hex) / 2;
    uint8_t *buf = malloc(len);
    if (!buf) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < len; i++) {
        char byte_str[3] = {hex[2*i], hex[2*i+1], '\0'};
        buf[i] = (uint8_t)strtol(byte_str, NULL, 16);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        free(buf);
        return EXIT_FAILURE;
    }

    // Установка таймаута, чтобы клиент не висел вечно
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    // Отправка (UDP)
    sendto(sock, buf, len, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    free(buf);

    // Получение ответа
    char resp[1024];
    ssize_t n = recvfrom(sock, resp, sizeof(resp) - 1, 0, NULL, NULL);
    if (n < 0) {
        fprintf(stderr, "Ошибка получения ответа (таймаут?)\n");
        close(sock);
        return EXIT_FAILURE;
    }

    resp[n] = '\0';
    printf("Ответ сервера: %s", resp);

    close(sock);
    return 0;
}
