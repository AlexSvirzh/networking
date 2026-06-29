# Advanced C & Network Programming

*🌍 [English](#english) | 🇷🇺 [Русский](#russian)*

---

<h2 id="english">🇬🇧 English</h2>

Projects focusing on low-level networking, inter-process communication (IPC), and dynamic library loading.

*   **🌐 NAT Traversal (UDP Hole Punching):** Python implementation bypassing NAT using a rendezvous server.
*   **🔄 Multi-Process UDP Server:** A robust daemonized C server. Uses `fork()` for concurrency, anonymous shared memory (`mmap`) + C11 atomics for lock-free IPC statistics, and safe signal handling (`sigaction`). Checked with Valgrind.
*   **🔌 Dynamic Plugin Loader:** A C utility that uses `dlopen`/`dlsym` to load search criteria plugins at runtime. Parses CLI args with `getopt_long` and handles Endianness conversions.
*   **📁 Recursive File Search:** Uses POSIX `ftw` to traverse directories and search for specific byte sequences in files.

---

<h2 id="russian">🇷🇺 Русский</h2>

Проекты, посвященные низкоуровневому сетевому программированию, межпроцессному взаимодействию (IPC) и динамической загрузке библиотек.

*   **🌐 NAT Traversal (UDP Hole Punching):** P2P соединение на Python за NAT с использованием промежуточного сервера.
*   **🔄 Многопроцессный UDP-сервер:** Демонизированный сервер на C. Использует `fork()`, анонимную разделяемую память (`mmap`) и атомарные переменные C11 для сбора статистики без блокировок. Проверен через Valgrind.
*   **🔌 Динамический загрузчик плагинов:** Утилита на C, использующая `dlopen`/`dlsym` для подгрузки логики парсинга в рантайме.
*   **📁 Рекурсивный поиск файлов:** Использование POSIX `ftw` для обхода каталогов и побайтового поиска в бинарных файлах.
