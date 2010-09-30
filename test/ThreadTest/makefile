all:     thread_test

sync_ops.o:     sync_ops.cpp sync_ops.h
	gcc -c sync_ops.cpp

thread_ops.o:     thread_ops.cpp thread_ops.h
	gcc -c thread_ops.cpp

thread_test.o:     thread_test.cpp
	gcc -c thread_test.cpp

thread_test:     sync_ops.o thread_ops.o thread_test.o
	g++ -o thread_test sync_ops.o thread_ops.o thread_test.o -lpthread

clean: 
	rm -f *.o thread_test a.out
