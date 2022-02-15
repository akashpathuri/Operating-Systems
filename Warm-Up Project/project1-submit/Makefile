all: sig bit threads

sig: signal.c
		gcc -m32 -g signal.c -o signal

threads: threads.c
		gcc -Wall -pthread threads.c -o threads	

bit: bit-shifting.c
		gcc -Wall  bit-shifting.c -o bitshift	
clean: 
		rm -f signal
		rm -f bit-shifting
		rm -f threads