#include "usf.h"
#include "audio_hle.h"
#include "memory.h"
#include "audio.h"
#include <stdlib.h>
#include <stdio.h>

int32_t SampleRate = 0;
static int16_t samplebuf[16384];

void OpenSound(void)
{
}

void AddBuffer(unsigned char *buf, unsigned int length) {
	int32_t i = 0, out = 0;
	
	if(!cpu_running)
		return;
		
	for(out = i = 0; i < (length >> 1); i+=2)
	{
		samplebuf[out++] = ((int16_t*)buf)[i+1];
		samplebuf[out++] = ((int16_t*)buf)[i];
	}
	
	fwrite( &SampleRate, sizeof(int32_t), 1, stdout );
	fwrite( &out, sizeof(int32_t), 1, stdout );
	fwrite( samplebuf, sizeof(int16_t), out, stdout );
	fflush( stdout );

	if ( fread( &out, sizeof(int32_t), 1, stdin ) < 1 )
		out = 0;

	cpu_running = out;
}

void AiLenChanged(void) {
	int32_t length = 0;
	uint32_t address = (AI_DRAM_ADDR_REG & 0x00FFFFF8);

	length = AI_LEN_REG & 0x3FFF8;

	AddBuffer(RDRAM+address, length);

	if(length && !(AI_STATUS_REG&0x80000000)) {
		const float VSyncTiming = 789000.0f;
		double BytesPerSecond = 48681812.0 / (AI_DACRATE_REG + 1) * 4;
		double CountsPerSecond = (double)((((double)VSyncTiming) * (double)60.0)) * 2.0;
		double CountsPerByte = (double)CountsPerSecond / (double)BytesPerSecond;
		unsigned int IntScheduled = (unsigned int)((double)AI_LEN_REG * CountsPerByte);

		ChangeTimer(AiTimer,IntScheduled);
	}

	if(enableFIFOfull) {
		if(AI_STATUS_REG&0x40000000)
			AI_STATUS_REG|=0x80000000;
	}

	AI_STATUS_REG|=0x40000000;

}

unsigned int AiReadLength(void) {
	AI_LEN_REG = 0;
	return AI_LEN_REG;
}

void AiDacrateChanged(unsigned  int value) {
	AI_DACRATE_REG = value;
	SampleRate = 48681812 / (AI_DACRATE_REG + 1);

}
