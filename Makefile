FLAGS = -Wall -Wextra -g

main.exe: main.o json_parser.o
	gcc $(FLAGS) $^ -o $@

main.o: main.c
	gcc $(FLAGS) -c $< -o $@ -I"../"

json_parser.o: json_parser.c
	gcc $(FLAGS) -c $< -o $@ -I"../"

clean:
	rm *.o
	rm main.exe

.PHONY: clean
