#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Структура для разделяемой памяти (статистика)
typedef struct {
    _Atomic unsigned long success_count;
    _Atomic unsigned long error_count;
    time_t start_time;
} server_stats_t;

// Глобальные переменные
const char *log_file = "/tmp/lab3.log";
bool debug_mode = false;
server_stats_t *stats = NULL;
volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t print_stats_flag = 0;

// Функция для записи в лог-файл
void write_log(const char *msg) {
    FILE *f = fopen(log_file, "a");
    if (!f) return;

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char time_buf[26];
    strftime(time_buf, sizeof(time_buf), "%d.%m.%y %H:%M:%S", tm_info);

    fprintf(f, "[%s] %s\n", time_buf, msg);
    fclose(f);
}

// Обработчики сигналов
void sig_shutdown(int signum) {
    (void)signum;
    keep_running = 0;
}

void sig_usr1(int signum) {
    (void)signum;
    print_stats_flag = 1;
}

// Обработчик SIGCHLD для уборки зомби-процессов
void sig_chld(int signum) {
    (void)signum;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Вывод статистики
void print_statistics() {
    time_t now = time(NULL);
    double uptime = difftime(now, stats->start_time);
    char msg[256];

    snprintf(msg, sizeof(msg),
             "СТАТИСТИКА: Время работы: %.0f сек | Успешных: %lu | Ошибок: %lu",
             uptime,
             atomic_load(&stats->success_count),
             atomic_load(&stats->error_count));

    // Вывод в лог и в stderr
    write_log(msg);
    fprintf(stderr, "%s\n", msg);
}

// Функция демонизации
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Родитель завершается

    if (setsid() < 0) exit(EXIT_FAILURE); // Становимся лидером сессии

    pid = fork(); // Второй форк для отвязки от терминала
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    if (chdir("/") < 0) exit(EXIT_FAILURE); // Переходим в корень

    // Закрываем stdin и stdout, но stderr оставляем для вывода статистики
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        if (fd > 2) close(fd);
    }
}

// Обработка данных
void handle_request(uint8_t *buf, ssize_t len, int sock, struct sockaddr_in *cli_addr, socklen_t cli_len, int wait_time) {
    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg),
             "Новый запрос от %s:%d (размер: %zd байт)",
             inet_ntoa(cli_addr->sin_addr),
             ntohs(cli_addr->sin_port),
             len);
    write_log(log_msg);

    // Имитация долгой работы (-w)
    if (wait_time > 0) {
        sleep(wait_time);
    }

    if (len < 2 || len > 256) {
        atomic_fetch_add(&stats->error_count, 1);
        sendto(sock, "ERROR 1\n", 8, 0, (struct sockaddr*)cli_addr, cli_len);
        write_log("Завершение: Ошибка 1 (Размер пакета выходит за рамки 2-256 байт)");
        return;
    }

    // Совпадение заявленной длины и реального размера пакета
    uint8_t data_len = buf[0];
    if (len != (ssize_t)(data_len + 1)) {
        atomic_fetch_add(&stats->error_count, 1);
        sendto(sock, "ERROR 2\n", 8, 0, (struct sockaddr*)cli_addr, cli_len);
        write_log("Завершение: Ошибка 2 (Указанная длина не совпадает с реальным размером данных)");
        return;
    }

    // Формирование ответа
    size_t out_len = data_len * 9;
    char *out_buf = malloc(out_len + 1);
    if (!out_buf) {
        atomic_fetch_add(&stats->error_count, 1);
        sendto(sock, "ERROR 3\n", 8, 0, (struct sockaddr*)cli_addr, cli_len);
        write_log("Завершение: Ошибка 3 (Ошибка выделения памяти)");
        return;
    }

    for (int i = 0; i < data_len; i++) {
        uint8_t b = buf[i + 1];
        for (int bit = 7; bit >= 0; bit--) {
            out_buf[i * 9 + (7 - bit)] = (b & (1 << bit)) ? '1' : '0';
        }
        out_buf[i * 9 + 8] = (i == data_len - 1) ? '\n' : ' ';
    }
    out_buf[out_len] = '\0';

    sendto(sock, out_buf, out_len, 0, (struct sockaddr*)cli_addr, cli_len);
    free(out_buf);
    atomic_fetch_add(&stats->success_count, 1);
    write_log("Завершение: Запрос успешно обработан");
}

int main(int argc, char *argv[]) {
    // Дефолтные настройки
    int port = 8080;
    char *ip_addr = "127.0.0.1";
    int wait_time = 0;
    bool is_daemon = false;
    log_file = "/tmp/lab3.log";
    debug_mode = (getenv("LAB3DEBUG") != NULL);

    // Считывание переменных окружения
    if (getenv("lab3WAIT")) wait_time = atoi(getenv("lab3WAIT"));
    if (getenv("lab3LOGFILE")) log_file = getenv("lab3LOGFILE");
    if (getenv("lab3ADDR")) ip_addr = getenv("lab3ADDR");
    if (getenv("lab3PORT")) port = atoi(getenv("lab3PORT"));

    // Парсинг опций
    int opt;
    while ((opt = getopt(argc, argv, "w:dl:a:p:vh")) != -1) {
        switch (opt) {
            case 'w': wait_time = atoi(optarg); break;
            case 'd': is_daemon = true; break;
            case 'l': log_file = optarg; break;
            case 'a': ip_addr = optarg; break;
            case 'p': port = atoi(optarg); break;
            case 'v':
                printf("Версия: 1.0\nВыполнил: Свиржевский А.Д., N3252\nВариант 16\n");
                return 0;
            case 'h':
                printf("Опции: -w <сек>, -d (демон), -l <лог>, -a <IP>, -p <порт>, -v, -h\n");
                return 0;
            default:
                return EXIT_FAILURE;
        }
    }

    if (is_daemon) daemonize();

    // Создаем разделяемую память для счетчиков
    stats = mmap(NULL, sizeof(server_stats_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (stats == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }

    stats->success_count = 0;
    stats->error_count = 0;
    stats->start_time = time(NULL);

    // Настройка обработки сигналов
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_shutdown;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = sig_usr1;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sig_chld;
    sa.sa_flags = SA_RESTART; // Чтобы не прерывать recvfrom при завершении дочернего процесса
    sigaction(SIGCHLD, &sa, NULL);

    // Создание UDP сокета
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_addr);
    serv_addr.sin_port = htons(port);

    // Позволяет запускать несколько экземпляров
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        return EXIT_FAILURE;
    }

    write_log("Сервер запущен (UDP)");

    // Главный цикл сервера
    while (keep_running) {
        if (print_stats_flag) {
            print_statistics();
            print_stats_flag = 0;
        }

        uint8_t buf[1024];
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cli_addr, &cli_len);
        if (n < 0) {
            if (errno == EINTR) continue; // Прервано сигналом
            perror("recvfrom");
            continue;
        }

        // Многопроцессность
        pid_t pid = fork();
        if (pid == 0) {
            // Дочерний процесс
            handle_request(buf, n, sock, &cli_addr, cli_len, wait_time);
            exit(EXIT_SUCCESS); // Завершается после обработки
        } else if (pid < 0) {
            perror("fork");
        }
    }

    write_log("Сервер остановлен");
    close(sock);
    munmap(stats, sizeof(server_stats_t));

    return 0;
}
