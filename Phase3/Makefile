CC=gcc
CFAGS=-g -Wall
TARGET=myshell
OBJECTS=myshell.o

all : $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm *.o myshell