CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

ordena: ordena.c
	$(CC) $(CFLAGS) -o ordena ordena.c

clean:
	rm -f ordena
