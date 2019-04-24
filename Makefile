all:
		gcc -pthread -Wall -Wextra -pedantic -o shell shell.c

zip:
	zip xkrajn02.zip Makefile shell.c

clean:
		rm -f shell xkrajn02.zip
