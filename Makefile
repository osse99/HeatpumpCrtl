TARGET = pump_controller
LIBS = -lmariadb -lwiringPi -lconfig
CC = gcc
CFLAGS = -g -Wall -D DEBUG=1 -D SIMULATE=1

.PHONY: default all clean

default:	$(TARGET)
all:	default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o:	%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS:	$(TARGET) $(OBJECTS)

$(TARGET):	$(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
