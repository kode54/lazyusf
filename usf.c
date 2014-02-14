
#include <stdint.h>
#include "usf.h"
#include "cpu.h"
#include "memory.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>

#include "types.h"

uint32_t cpu_running = 0, use_interpreter = 0, use_audiohle = 0, is_paused = 0, cpu_stopped = 1, fake_seek_stopping = 0;

uint32_t enablecompare = 0, enableFIFOfull = 0;

uint32_t usf_length = 0, usf_fade_length = 0;

int LoadUSF()
{
	uint32_t reserved_size, temp;

	fread( &enablecompare, sizeof(uint32_t), 1, stdin );
	fread( &enableFIFOfull, sizeof(uint32_t), 1, stdin );

	fread( &reserved_size, sizeof(uint32_t), 1, stdin );

	while ( reserved_size ) {
		fread( &temp, sizeof(uint32_t), 1, stdin );
		if(temp == 0x34365253) { //there is a rom section
			uint32_t len = 0, start = 0;
			fread(&len, sizeof(uint32_t), 1, stdin);

			while(len) {
				fread(&start, sizeof(uint32_t), 1, stdin);

				while(len) {
					int page = start >> 16;
					int readLen = ( ((start + len) >> 16) > page) ? (((page + 1) << 16) - start) : len;

					if(ROMPages[page] == 0) {
						ROMPages[page] = malloc(0x10000);
						memset(ROMPages[page], 0, 0x10000);
                			}

					fread(ROMPages[page] + (start & 0xffff), readLen, 1, stdin);

					start += readLen;
					len -= readLen;
				}

				fread(&len, sizeof(uint32_t), 1, stdin);
			}

		}

		fread(&temp, sizeof(uint32_t), 1, stdin);
		if(temp == 0x34365253) {
			uint32_t len = 0, start = 0;
			fread(&len, sizeof(uint32_t), 1, stdin);

			while(len) {
				fread(&start, sizeof(uint32_t), 1, stdin);

				fread(savestatespace + start, len, 1, stdin);

				fread(&len, sizeof(uint32_t), 1, stdin);
			}
		}

		fread( &reserved_size, sizeof(uint32_t), 1, stdin );
	}

    // Detect the Ramsize before the memory allocation 
	
	if(*(uint32_t*)(savestatespace + 4) == 0x400000) {
		RdramSize = 0x400000;
		savestatespace = realloc(savestatespace, 0x40275c);
	} else if(*(uint32_t*)(savestatespace + 4) == 0x800000)
		RdramSize = 0x800000;

	return 1;
}


void usf_init()
{
	use_audiohle = 0;
}

void usf_destroy()
{

}

void usf_play()
{
	uint32_t i = 0;

	savestatespace = NULL;
	cpu_running = is_paused = fake_seek_stopping = 0;
	cpu_stopped = 1;

    // Allocate main memory after usf loads  (to determine ram size)
	
	PreAllocate_Memory();

    if(!LoadUSF()) {
		Release_Memory();
    }
	
	Allocate_Memory();
	
    do {		
		StartEmulationFromSave(savestatespace);		
	} while (cpu_running);
	
	Release_Memory();

}

int main(void)
{
	usf_init();
	usf_play();
	return 0;
}
