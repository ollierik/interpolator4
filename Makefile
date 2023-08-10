all: test
	./test

test: test.c
	clang -std=c89 -o test test.c

clean:
	rm -f test
