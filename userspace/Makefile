test_sources= $(wildcard ./test/*.c) $(wildcard ./test/*.h)
logger_sources= $(wildcard ./logic/logger/*.c) $(wildcard ./logic/logger/*.h)
binDir= ./bin

DEBUG=-g -DLOG_LEVEL=3 -Wall -Wextra
CC=gcc
INCLUDES=-I./logic

$(binDir)/test: $(test_sources) $(logger_sources)
	mkdir -p $(binDir)
	$(CC) $(FLAGS) $(DEBUG) $(INCLUDES) $^ -o $@ 