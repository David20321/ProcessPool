objects = processpool.o test.o

test : $(objects)
	gcc -o test $(objects) -lstdc++ -lpthread

processpool.o : processpool.h disallow_copy_and_assign.h
test.o : processpool.h disallow_copy_and_assign.h

.PHONY : clean
clean :
	rm test $(objects)
