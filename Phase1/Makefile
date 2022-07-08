CC=gcc
CFAGS=-g
TARGET=myshell
OBJECTS=myshell.o

all : $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean: 
	rm *.o myshell