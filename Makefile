CFLAGS = -w -c
LDFLAGS = -o lazyusf

OBJS = audio.o cpu.o dma.o exception.o interpreter_cpu.o interpreter_ops.o main.o pif.o registers.o tlb.o usf.o memory.o rsp.o


GCC = clang
GPP = clang++
LD =  clang++


OPTS = -O3
ROPTS = -O3 -DARCH_MIN_SSE2

all: lazyusf

lazyusf : $(OBJS)
	$(LD) -g $(LDFLAGS) $^

.c.o:
	$(GCC) $(CFLAGS) $(OPTS) $*.c

.cpp.o:
	$(GPP) $(CFLAGS) $(OPTS) $*.cpp

rsp.o: rsp/rsp.c
	$(GCC) $(CFLAGS) $(ROPTS) rsp/rsp.c

clean:
	rm -f $(OBJS) lazyusf > /dev/null


