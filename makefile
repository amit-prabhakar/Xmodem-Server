PORT=50429

all : client1 xmodemserver

client1 : client1.o crc16.o
	gcc -DPORT=$(PORT) -g -Wall -o client1 client1.o crc16.o

xmodemserver : xmodemserver.o crc16.o
	gcc -DPORT=$(PORT) -g -Wall -o xmodemserver xmodemserver.o crc16.o

crc16.o : crc16.c crc16.h
	gcc -DPORT=$(PORT) -g -Wall -c crc16.c

client1.o : client1.c crc16.h
	gcc -DPORT=$(PORT) -g -Wall -c client1.c

xmodemserver.o : xmodemserver.c xmodemserver.h crc16.h
	gcc -DPORT=$(PORT) -g -Wall -c xmodemserver.c

clean:
	rm *.o client1 xmodemserver