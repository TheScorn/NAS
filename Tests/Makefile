CFLAGS = -c -o

Test.out: Test.o NAS_argument_handler.o helper_functions.o
	@g++ -o $@ Test.o NAS_argument_handler.o helper_functions.o -lgtest -lgtest_main -pthread

Test.o: Test.cpp
	@g++ -I /usr/include $(CFLAGS) $@ $<

NAS_argument_handler.o: ../NAS_argument_handler.c
	@gcc $(CFLAGS) NAS_argument_handler.o ../NAS_argument_handler.c

helper_functions.o: ../helper_functions.c
	@gcc $(CFLAGS) helper_functions.o ../helper_functions.c

.PHONY: clean
clean:
	@rm -f *.o Test.out