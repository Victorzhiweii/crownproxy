CrownTester: main.o uart.o sock.o crc.o cJSON.o
	arm-linux-gnueabihf-gcc -o CrownTester uart.o sock.o  main.o crc.o cJSON.o -lpthread -lm
uart.o: uart.c
	arm-linux-gnueabihf-gcc -c uart.c
main.o: main.c
	arm-linux-gnueabihf-gcc -c main.c
sock.o: sock.c
	arm-linux-gnueabihf-gcc -c sock.c	
crc.o: crc.c
	arm-linux-gnueabihf-gcc -c crc.c
cJSON.o: cJSON.c
	arm-linux-gnueabihf-gcc -c cJSON.c
clean:
	rm CrownTester
distclean: clean
	find -name '*.o' | xargs rm -f
