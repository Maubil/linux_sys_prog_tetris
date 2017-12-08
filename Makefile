CLIENT_EXEC = client
SERVER_EXEC = server
TEST_EXEC = test
COMMON = $(wildcard src/*.c)
CLIENT_SOURCES = $(wildcard src/client/*.c)
SERVER_SOURCES = $(wildcard src/server/*.c)
TEST_SOURCES = $(wildcard src/test/*.c)
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)
COMMON_OBJECTS = $(COMMON:.c=.o)
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

CC ?= gcc
CC_FLAGS ?= -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L
LD_FLAGS ?= -lm -lncurses -lpthread

all: $(CLIENT_EXEC) $(SERVER_EXEC) $(TEST_EXEC)

$(CLIENT_EXEC): $(CLIENT_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $(CLIENT_OBJECTS) $(COMMON_OBJECTS) -o $(CLIENT_EXEC) $(LD_FLAGS)

$(SERVER_EXEC): $(SERVER_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $(SERVER_OBJECTS) $(COMMON_OBJECTS) -o $(SERVER_EXEC) $(LD_FLAGS)

$(TEST_EXEC): $(TEST_OBJECTS) $(COMMON_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(COMMON_OBJECTS) -o $(TEST_EXEC) $(LD_FLAGS)

%.o: %.c
	$(CC) -c $(CC_FLAGS) $< -o $@

clean:
	rm -f $(CLIENT_EXEC) $(SERVER_EXEC) $(TEST_EXEC) $(CLIENT_OBJECTS) $(SERVER_OBJECTS) $(TEST_OBJECTS) $(COMMON_OBJECTS)
