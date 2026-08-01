CFLAGS = -c -o

Test.out: Test.o NAS_argument_handler.o
	@g++ -o $@ Test.o NAS_argument_handler.o -lgtest -lgtest_main -pthread

Test.o: Test.cpp
	@g++ -I /usr/include $(CFLAGS) $@ $<

NAS_argument_handler.o: ../NAS_argument_handler.c
	@gcc $(CFLAGS) NAS_argument_handler.o ../NAS_argument_handler.c

.PHONY: clean
clean:
	@rm -f *.o Test.out