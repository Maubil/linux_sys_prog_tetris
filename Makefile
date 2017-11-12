EXEC = demo
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)

CC ?= gcc
CC_FLAGS ?= -std=c99 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200112L
LD_FLAGS ?= -lm

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LD_FLAGS)

%.o: %.c
	$(CC) -c $(CC_FLAGS) $< -o $@

clean:
	rm -f $(EXEC) $(OBJECTS)
