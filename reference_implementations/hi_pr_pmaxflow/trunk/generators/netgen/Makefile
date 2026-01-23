CFLAGS = -O -DDIMACS -fPIC -std=c99

OBJS = netgen.o index.o random.o

netgen: $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

netgen.o: netgen.c netgen.h

index.o: index.c netgen.h

random.o: random.c
