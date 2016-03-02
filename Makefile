CC=gcc
CFLAGS=-Wall -pedantic

Q1_SENDER_SOURCE=q1sender.c sender.h
Q1_RECEIVER_SOURCE=q1receiver.c
Q1_SENDER_EXEC=q1sender
Q1_RECEIVER_EXEC=q1receiver

Q2_SENDER_SOURCE=q2sender.c sender.h
Q2_RECEIVER_SOURCE=q2receiver.c
Q2_SENDER_EXEC=q2sender
Q2_RECEIVER_EXEC=q2receiver

EXEC=$(Q1_SENDER_EXEC) $(Q1_RECEIVER_EXEC) $(Q2_SENDER_EXEC) $(Q2_RECEIVER_EXEC)

all: q1sender q1receiver q2sender q2receiver

q1sender: $(Q1_SENDER_SOURCE)
	$(CC) $(CFLAGS) -o $(Q1_SENDER_EXEC) $(Q1_SENDER_SOURCE)

q1receiver: $(Q1_RECEIVER_SOURCE)
	$(CC) $(CFLAGS) -o $(Q1_RECEIVER_EXEC) $(Q1_RECEIVER_SOURCE)
	
q2sender: $(Q2_SENDER_SOURCE)
	$(CC) $(CFLAGS) -o $(Q2_SENDER_EXEC) $(Q2_SENDER_SOURCE)

q2receiver: $(Q2_RECEIVER_SOURCE)
	$(CC) $(CFLAGS) -o $(Q2_RECEIVER_EXEC) $(Q2_RECEIVER_SOURCE)

clean:
	rm -f *.o $(EXEC) *~