SHELL := /bin/bash
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE
SERVER = lab3asdN3252_server
CLIENT = lab3asdN3252_client

# Флаги Valgrind (отслеживание форков и запись логов по PID)
VFLAGS = valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --log-file=valgrind_%p.txt

all: $(SERVER) $(CLIENT)

$(SERVER): server.c
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT): client.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(SERVER) $(CLIENT) valgrind*.txt
	rm -f /tmp/lab3.log

test: all
	@echo -e "\n 1. ЗАПУСК СЕРВЕРА (Демон, порт 8080) "
	rm -f /tmp/lab3.log
	./$(SERVER) -d -p 8080 -a 127.0.0.1
	@sleep 1
	@echo -e "\n 2. ОТПРАВКА КОРРЕКТНОГО ЗАПРОСА "
	./$(CLIENT) 127.0.0.1 8080 0x02AABB
	@echo -e "\n 3. ТЕСТ: СТРОГОСТЬ ПРОТОКОЛА "
	@echo "[Тест 3.1] Пакет короче 2 байт (только длина):"
	./$(CLIENT) 127.0.0.1 8080 0x05 || true
	@echo "[Тест 3.2] Пакет слишком длинный (указано 2, по факту 4):"
	./$(CLIENT) 127.0.0.1 8080 0x02AABBCCDD || true
	@echo "[Тест 3.3] Пакет слишком короткий (указано 5, по факту 2):"
	./$(CLIENT) 127.0.0.1 8080 0x05AABB || true
	@echo -e "\n 4. ТЕСТ: ИСТИННАЯ МНОГОЗАДАЧНОСТЬ (fork) "
	@echo "Запускаем второй сервер на порту 8081 с задержкой обработки 3 секунды..."
	./$(SERVER) -d -p 8081 -a 127.0.0.1 -w 3
	@sleep 1
	@echo "Отправляем долгий запрос (сервер уснет на 3 сек)..."
	./$(CLIENT) 127.0.0.1 8081 0x01FF &
	@sleep 0.5
	@echo "Сразу отправляем быстрый запрос..."
	./$(CLIENT) 127.0.0.1 8081 0x01AA
	@echo "Ждем завершения первого запроса..."
	@wait
	@echo -e "\n 5. ТЕСТ: ПРИОРИТЕТ ПЕРЕМЕННЫХ ОКРУЖЕНИЯ "
	@echo "Задаем lab3PORT=9000, но передаем опцию -p 8082."
	@echo "Ожидание: сервер должен слушать 8082 (опция приоритетнее)."
	lab3PORT=9000 ./$(SERVER) -d -p 8082 -a 127.0.0.1
	@sleep 1
	./$(CLIENT) 127.0.0.1 8082 0x01BB
	@echo -e "\n 6. ВЫЗОВ СТАТИСТИКИ (SIGUSR1) "
	killall -SIGUSR1 $(SERVER) || true
	@sleep 1
	@echo -e "\n 7. ОСТАНОВКА ВСЕХ СЕРВЕРОВ (SIGTERM) "
	killall -SIGTERM $(SERVER) || true
	@sleep 1
	@echo -e "\n 8. ИТОГОВЫЙ ЛОГ ФАЙЛ "
	@cat /tmp/lab3.log

valgrind_test: all
	@echo -e "\n ТЕСТИРОВАНИЕ УТЕЧЕК ПАМЯТИ (VALGRIND) "
	@rm -f valgrind*.txt /tmp/lab3.log
	@echo "[+] Запуск сервера под Valgrind..."
	$(VFLAGS) ./$(SERVER) -d -p 8080 -a 127.0.0.1
	@sleep 2
	@echo "[+] Отправка валидного запроса клиентом (клиент тоже под Valgrind)..."
	valgrind --leak-check=full --log-file=valgrind_client_%p.txt ./$(CLIENT) 127.0.0.1 8080 0x02AABB
	@echo "[+] Отправка невалидного запроса..."
	./$(CLIENT) 127.0.0.1 8080 0x05 || true
	@sleep 1
	@echo "[+] Остановка сервера для выгрузки отчетов Valgrind..."
	pkill -SIGTERM -f "[l]ab3asdN3252_server" || true
	@sleep 2
	@echo -e "\n СВОДКА ОШИБОК И УТЕЧЕК VALGRIND "
	@grep -H -E "ERROR SUMMARY|definitely lost|possibly lost" valgrind*.txt || echo "Логи не найдены или пусты"
	@echo -e "\nПодробные отчеты сохранены в файлах valgrind_<PID>.txt"

archive: clean
	cd .. && tar -czvf $(SERVER).tar.gz lab3asdN3252/
	@echo -e "\n Архив готов! "
