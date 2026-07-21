CC = gcc
CCFLAGS = -lstdc++ -std=c++17 -Wall -Wextra
LIBS = -pthread

.PHONY: all library app clean run

all: library app

library: lib/liblogger.so

app: logger_app

lib/liblogger.so: logger.o
	mkdir -p lib
	$(CC) -shared -o lib/liblogger.so logger.o $(LIBS)
	@echo "Библиотека собрана: lib/liblogger.so"

logger.o: logger.cpp
	$(CC) $(CCFLAGS) -fPIC $(LIBS) -c logger.cpp -o logger.o

main.o: main.cpp
	$(CC) $(CCFLAGS) -c main.cpp -o main.o

logger_app: main.o lib/liblogger.so
	$(CC) -o logger_app main.o -lstdc++ -Llib -llogger $(LIBS) -Wl,-rpath,./lib
	@echo "Приложение собрано"

clean:
	rm -rf lib *.o logger_app test.log
	@echo "Очищено"

run: all
	./logger_app test.log INFO
