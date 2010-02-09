CFLAGS = -w -c -DUSEX64 -DEXT_REGS -ffixed-R15 `pkg-config --cflags audacious` -fPIC -g
LDFLAGS = -o lazyusf.so -shared `pkg-config --libs audacious` -fPIC 

OBJS = audio.o audio_hle_main.o audio_ucode1.o audio_ucode2.o audio_ucode3.o audio_ucode3mp3.o cpu.o dma.o exception.o interpreter_cpu.o interpreter_ops.o main.o pif.o psftag.o recompiler_fpu_ops.o recompiler_ops.o registers.o rsp_mmx.o rsp_recompiler_analysis.o rsp_recompiler_ops.o rsp_sse.o rsp_x86.o tlb.o x86.o x86_fpu.o usf.o


GCC = gcc
GPP = g++
LD =  g++

OPTS = -g
ROPTS = -g

all: lazyusf.so

install: all
	cp lazyusf.so /usr/lib/audacious/Input

lazyusf.so : $(OBJS) recompiler_cpu.o memory.o rsp.o rsp_interpreter_cpu.o rsp_recompiler_cpu.o
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
	rm -f $(OBJS) recompiler_cpu.o memory.o rsp.o rsp_interpreter_cpu.o rsp_recompiler_cpu.o lazyusf.so > /dev/null


