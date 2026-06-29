SHELL := /bin/bash
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_GNU_SOURCE

TARGET = lab2afN3251
PLUGIN = libafN3251.so

all: $(TARGET) $(PLUGIN)

$(TARGET): main.c plugin_api.h
	$(CC) $(CFLAGS) -o $@ main.c -ldl

$(PLUGIN): libafN3251.c plugin_api.h
	$(CC) $(CFLAGS) -shared -fPIC -o $@ libafN3251.c

clean:
	chmod 777 test_dir/no_access.bin test_dir/no_access_dir 2>/dev/null || true
	rm -f $(TARGET) $(PLUGIN) valgrind.txt analyzer.txt
	rm -rf test_dir

test: all
	@echo -e "\n"
	@echo "~~~~~~ Шаг 1. ПОДГОТОВКА ПОЛИГОНА ~~~~~~"

	rm -rf test_dir && mkdir -p test_dir/sub_dir test_dir/custom_plugins test_dir/no_access_dir
	# Искомая строка: 0b110110100001 (12 бит). 
	# Запишем байты \xDA\x10. В бинарном виде это: 11011010 00010000. 
	# Первые 12 бит ИДЕАЛЬНО совпадают с 0b110110100001!
	printf "\xDA\x10" > test_dir/match_binary.bin
	# А теперь запишем \x06\xD0\x80. Биты: 00000110 11010000 10000000
	# Искомая строка 110110100001 находится тут начиная с 5-го бита (считая с 0):
	# 00000[110110100001]0000000
	printf "\x06\xD0\x80" > test_dir/sub_dir/match_shifted.bin
	
	# Файл без совпадения
	printf "\x11\x22\x33\x44" > test_dir/no_match.bin
	# Файлы с ошибками доступа
	echo "secret" > test_dir/no_access.bin
	chmod 000 test_dir/no_access.bin
	chmod 000 test_dir/no_access_dir
	
	@echo -e "\n"
	@echo "~~~~~~ Шаг 2. ТЕСТ ПОИСКА ПО БИТАМ (Разные форматы ввода) ~~~~~~"

	@echo "Ожидаем найти 2 файла, передав параметр через 0b..."
	LAB2DEBUG=1 ./$(TARGET) --bit-seq 0b110110100001 test_dir
	@echo -e "\nОжидаем найти те же 2 файла, передав через 0x... (0xDA1)"
	./$(TARGET) --bit-seq 0xDA1 test_dir
	@echo -e "\nОжидаем найти те же 2 файла, передав десятичное (3489)"
	./$(TARGET) --bit-seq 3489 test_dir

	@echo -e "\n"
	@echo "~~~~~~ Шаг 3. ИНВЕРСИЯ ПОИСКА (Опция -N) ~~~~~~"

	@echo "Ожидаем найти: no_match.bin"
	./$(TARGET) -N --bit-seq 0b110110100001 test_dir

	@echo -e "\n"
	@echo "~~~~~~ Шаг 4. VALGRIND (ПРОВЕРКА УТЕЧЕК ПАМЯТИ) ~~~~~~"

	valgrind --leak-check=full --show-leak-kinds=all --log-file=valgrind.txt ./$(TARGET) --bit-seq 0b110110100001 test_dir || true
	@grep "HEAP SUMMARY:" valgrind.txt -A 5 || true

	@echo -e "\n"
	@echo "~~~~~~ ТЕСТИРОВАНИЕ ЗАВЕРШЕНО УСПЕШНО ~~~~~~"

	chmod 777 test_dir/no_access.bin test_dir/no_access_dir || true

analyze: clean
	$(CC) $(CFLAGS) -fanalyzer main.c -o $(TARGET) -ldl 2> analyzer.txt || true
	@echo "Статический анализ завершен. Результат в analyzer.txt"

archive: clean
	cd .. && tar -czvf $(TARGET).tar.gz $(TARGET)/
