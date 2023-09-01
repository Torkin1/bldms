test_sources= $(wildcard ./test/*.c) #$(wildcard ./test/*.h)
logic_sources= $(shell find ./logic -name "*.c")
binDir= ./bin

DEBUG=-g -DLOG_LEVEL=3 -Wall -Wextra
CC=gcc
INCLUDES=-I./logic

$(binDir)/test: $(test_sources) $(logic_sources)
	mkdir -p $(binDir)
	$(CC) $(FLAGS) $(DEBUG) $(INCLUDES) $^ -o $@ 