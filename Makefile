huffman: huffman.c
	gcc -std=c11 -Wall -Wextra -g -fsanitize=address -o huffman huffman.c

.PHONY: clean
clean:
	rm huffman

