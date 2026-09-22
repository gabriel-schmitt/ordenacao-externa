CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

all: particoes intercalacao

particoes: particoes.c
	$(CC) $(CFLAGS) -o particoes particoes.c

intercalacao: intercalacao.c
	$(CC) $(CFLAGS) -o intercalacao intercalacao.c

clean:
	rm -f particoes intercalacao
