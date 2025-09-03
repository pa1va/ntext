CC = cc
CFLAGS = -Wall -Wextra -pedantic -std=c99
LIBS = -lncurses
TARGET = main
OBJ = main.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
