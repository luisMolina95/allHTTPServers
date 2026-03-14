.PHONY: all clean run

all: http.out

http.out: main.c
	gcc main.c -o http.out

run: http.out
	./http.out

clean:
	rm -f http.out