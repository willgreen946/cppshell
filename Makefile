CC=c++
BIN=cshell

all:
	$(CC) src/main.cpp -O3 -g -o $(BIN)
	mkdir -p bin
	mv $(BIN) bin/$(BIN)

run:
	./$(BIN)
