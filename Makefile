objects = processpool.o

all : main worker

main : $(objects)
	gcc -o main main.cpp $(objects) -lstdc++ -lpthread

worker : $(objects)
	gcc -o worker worker.cpp $(objects) -lstdc++ -lpthread

processpool.o : processpool.h disallow_copy_and_assign.h

.PHONY : clean
clean :
	rm main worker $(objects)
