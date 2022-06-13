CC = gcc -pthread
exe = main
exe2 = main_MY_MALLOC
obj1 = main.o mymemalloc.o
obj2 = main_MY_MALLOC.o mymemalloc.o

all: $(obj1) $(obj2)
	$(CC) -o $(exe) $(obj1) 
	$(CC) -o $(exe2) $(obj2) 
mymemalloc.o: mymemalloc.c
	$(CC) -c $^ -o $@
main_MY_MALLOC.o main.o: main.c
	$(CC) -c main.c -o main.o
	$(CC) -DMY_MALLOC -c main.c -o main_MY_MALLOC.o



.PHONY: clean
clean:
	rm -rf $(obj1) $(obj2) $(exe) $(exe2)