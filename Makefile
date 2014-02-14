CFLAGS = -w -c -DUSEX64
LDFLAGS = -o lazyusf 

OBJS = audio.o audio_hle_main.o audio_ucode1.o audio_ucode2.o audio_ucode3.o audio_ucode3mp3.o cpu.o dma.o exception.o interpreter_cpu.o interpreter_ops.o main.o pif.o registers.o tlb.o usf.o memory.o rsp.o rsp_interpreter_cpu.o


GCC = clang
GPP = clang++
LD =  clang++


OPTS = -O2
ROPTS = -O1

all: lazyusf

lazyusf : $(OBJS)
	$(LD) -g $(LDFLAGS) $^

.c.o:
	$(GCC) $(CFLAGS) $(OPTS) $*.c

.cpp.o:
	$(GPP) $(CFLAGS) $(OPTS) $*.cpp

recompiler_cpu.o: recompiler_cpu.c
	$(GCC) $(CFLAGS) $(ROPTS) $*.c

rsp.o: rsp.c
	$(GCC) $(CFLAGS) $(ROPTS) $*.c

memory.o: memory.c
	$(GCC) $(CFLAGS) $(ROPTS) $*.c

rsp_interpreter_cpu.o: rsp_interpreter_cpu.c
	$(GCC) $(CFLAGS) $(ROPTS) $*.c

rsp_recompiler_cpu.o: rsp_recompiler_cpu.c
	$(GCC) $(CFLAGS) $(ROPTS) $*.c


clean:
	rm -f $(OBJS) recompiler_cpu.o memory.o rsp.o rsp_interpreter_cpu.o rsp_recompiler_cpu.o lazyusf > /dev/null


