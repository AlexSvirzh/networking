SHELL := /bin/bash
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE

TARGET = lab2asdN3252
PLUGIN = libasdN3252.so

all: $(TARGET) $(PLUGIN)

$(TARGET): main.c plugin_api.h
	$(CC) $(CFLAGS) -o $@ main.c -ldl

$(PLUGIN): libasdN3252.c plugin_api.h
	$(CC) $(CFLAGS) -shared -fPIC -o $@ libasdN3252.c

clean:
	chmod 777 test_dir/no_access.bin test_dir/no_access_dir 2>/dev/null || true
	rm -f $(TARGET) $(PLUGIN) valgrind.txt
	rm -rf test_dir

test: all
	@echo -e "\n"
	@echo " Шаг 1. ПОДГОТОВКА "
	rm -rf test_dir && mkdir -p test_dir/sub_dir test_dir/custom_plugins test_dir/no_access_dir
	# 192.168.8.1 в Big Endian (C0 A8 08 01)
	printf "\xC0\xA8\x08\x01\x99\x88" > test_dir/match_be.bin
	# 192.168.8.1 в Little Endian (01 08 A8 C0)
	printf "\x00\x01\x08\xA8\xC0\xFF" > test_dir/sub_dir/match_le.bin
	# Файл без адреса
	printf "\x11\x22\x33\x44" > test_dir/no_match.bin
	# Файлы с ошибками доступа
	echo "secret" > test_dir/no_access.bin
	chmod 000 test_dir/no_access.bin
	chmod 000 test_dir/no_access_dir
	@echo -e "\n"
	@echo " Шаг 2. ТЕСТ СТАНДАРТНЫХ ФЛАГОВ (-h и -v) "
	./$(TARGET) -v
	./$(TARGET) -h
	@echo -e "\n"
	@echo " Шаг 3. ТЕСТ ОШИБОК ПОЛЬЗОВАТЕЛЯ "
	@echo "[Ожидается ошибка: Неизвестная опция]"
	./$(TARGET) --unknown-opt test_dir || true
	@echo -e "\n[Ожидается ошибка: Некорректный IP-адрес]"
	LAB2DEBUG=1 ./$(TARGET) --ipv4-addr-bin 999.99.99.99 test_dir || true
	@echo -e "\n[Ожидается вывод справки: Нет каталога]"
	./$(TARGET) --ipv4-addr-bin 192.168.8.1 || true
	@echo -e "\n"
	@echo " Шаг 4. ПОИСК IP (Big & Little Endian) "
	@echo "Ожидаем найти: match_be.bin и match_le.bin"
	./$(TARGET) --ipv4-addr-bin 192.168.8.1 test_dir
	@echo -e "\n"
	@echo " Шаг 5. ИНВЕРСИЯ ПОИСКА (Опция -N) "
	@echo "Ожидаем найти: no_match.bin (файлы без прав доступа должны игнорироваться)"
	./$(TARGET) -N --ipv4-addr-bin 192.168.8.1 test_dir
	@echo -e "\n"
	@echo " Шаг 6. ИСПОЛЬЗОВАНИЕ КАСТОМНОЙ ДИРЕКТОРИИ (Опция -P) "
	cp $(PLUGIN) test_dir/custom_plugins/
	# Вызываем из другой папки, чтобы плагин не нашелся по умолчанию
	./$(TARGET) -P test_dir/custom_plugins --ipv4-addr-bin 192.168.8.1 test_dir
	@echo -e "\n"
	@echo " Шаг 7. VALGRIND (ПРОВЕРКА УТЕЧЕК ПАМЯТИ) "
	valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.txt ./$(TARGET) --ipv4-addr-bin 192.168.8.1 test_dir || true
	@grep "HEAP SUMMARY:" valgrind.txt -A 5 || true
	@echo -e "\n"
	@echo "ТЕСТИРОВАНИЕ ЗАВЕРШЕНО УСПЕШНО"
	chmod 777 test_dir/no_access.bin test_dir/no_access_dir || true

archive: clean
	cd .. && tar -czvf $(TARGET).tar.gz $(TARGET)/
