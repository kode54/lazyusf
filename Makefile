CFLAGS = -c -fPIC

OBJS = audio.o cpu.o dma.o exception.o interpreter_cpu.o interpreter_ops.o main.o pif.o registers.o tlb.o usf.o memory.o rsp.o alist.o alist_nead.o jpeg.o mp3.o alist_audio.o audio_hle.o main_hle.o musyx.o alist_naudio.o cicx105.o memory_hle.o plugin_hle.o


OPTS = -O3
ROPTS = -O3 -DARCH_MIN_SSE2

all: liblazyusf.a

liblazyusf.a : $(OBJS)
	$(AR) rcs $@ $^

.c.o:
	$(CC) $(CFLAGS) $(OPTS) $*.c

rsp.o: rsp/rsp.c
	$(CC) $(CFLAGS) $(ROPTS) rsp/rsp.c

alist.o: rsp_hle/alist.c
	$(CC) $(CFLAGS) $(OPTS) $^

alist_nead.o: rsp_hle/alist_nead.c
	$(CC) $(CFLAGS) $(OPTS) $^

jpeg.o: rsp_hle/jpeg.c
	$(CC) $(CFLAGS) $(OPTS) $^

mp3.o: rsp_hle/mp3.c
	$(CC) $(CFLAGS) $(OPTS) $^

alist_audio.o: rsp_hle/alist_audio.c
	$(CC) $(CFLAGS) $(OPTS) $^

audio_hle.o: rsp_hle/audio_hle.c
	$(CC) $(CFLAGS) $(OPTS) $^

main_hle.o: rsp_hle/main_hle.c
	$(CC) $(CFLAGS) $(OPTS) $^

musyx.o: rsp_hle/musyx.c
	$(CC) $(CFLAGS) $(OPTS) $^

alist_naudio.o: rsp_hle/alist_naudio.c
	$(CC) $(CFLAGS) $(OPTS) $^

cicx105.o: rsp_hle/cicx105.c
	$(CC) $(CFLAGS) $(OPTS) $^

memory_hle.o: rsp_hle/memory_hle.c
	$(CC) $(CFLAGS) $(OPTS) $^

plugin_hle.o: rsp_hle/plugin_hle.c
	$(CC) $(CFLAGS) $(OPTS) $^

clean:
	rm -f $(OBJS) liblazyusf.a > /dev/null

