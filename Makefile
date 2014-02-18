CFLAGS = -c -fPIC

OBJS = audio.o cpu.o dma.o exception.o interpreter_cpu.o interpreter_ops.o main.o pif.o registers.o tlb.o usf.o memory.o rsp.o


OPTS = -O3
ROPTS = -O3 -DARCH_MIN_SSE2

all: liblazyusf.a

liblazyusf.a : $(OBJS)
	$(AR) rcs $@ $^

.c.o:
	$(CC) $(CFLAGS) $(OPTS) $*.c

rsp.o: rsp/rsp.c
	$(CC) $(CFLAGS) $(ROPTS) rsp/rsp.c

clean:
	rm -f $(OBJS) liblazyusf.a > /dev/null

