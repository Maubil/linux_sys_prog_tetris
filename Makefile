EXEC = demo
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

CC ?= gcc
CC_FLAGS ?= -Wall -Wextra
LD_FLAGS ?= -lm

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXEC) $(LD_FLAGS)

%.o: %.c
	$(CC) -c $(CC_FLAGS) $< -o $@

clean:
	rm -f $(EXEC) $(OBJECTS)
