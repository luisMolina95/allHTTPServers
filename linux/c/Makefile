.PHONY: all clean run

FILE ?= main

all: $(FILE).out

$(FILE).out: $(FILE).c
	gcc $(FILE).c -o $(FILE).out

run: $(FILE).out
	./$(FILE).out

clean:
	rm -f $(FILE).out
