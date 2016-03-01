CC=gcc
CFLAGS=-Wall -pedantic
SENDER_SOURCE=sender.c sender.h
RECEIVER_SOURCE=receiver.c
SENDER_EXEC=sender
RECEIVER_EXEC=receiver
EXEC=$(SENDER_EXEC) $(RECEIVER_EXEC)

all: sender receiver

sender: $(SENDER_SOURCE)
	$(CC) $(CFLAGS) -o $(SENDER_EXEC) $(SENDER_SOURCE)

receiver: $(RECEIVER_SOURCE)
	$(CC) $(CFLAGS) -o $(RECEIVER_EXEC) $(RECEIVER_SOURCE)

clean:
	rm -f *.o $(EXEC)