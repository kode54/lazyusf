#include "usf.h"
#include "audio_hle.h"
#include "memory.h"
#include "audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>


#include <audacious/util.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>
#include <audacious/strings.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
extern InputPlayback * pcontext;

int32_t SampleRate = 0, fd = 0, firstWrite = 1, curHeader = 0;
int32_t bufptr = 0;
int32_t AudioFirst = 0;
int32_t first = 1;


void OpenSound(void)
{
  	/*if(fd != 0) {
  		printf("already open!\n");
  	}
  	fd = open("/dev/dsp", O_WRONLY);
	if (fd < 0) {
		perror("open of /dev/dsp failed");
		exit(1);
	}   */

	if (pcontext->output->open_audio(FMT_S16_LE,SampleRate,2) == 0)
		printf("FAIL!\n");
}

// playback->pass_audio(playback,FMT_S16_LE,vgmstream->channels , l , buffer , &playback->playing );


void AddBuffer(unsigned char *buf, unsigned int length) {
	pcontext->playing = 1;
  	pcontext->eof = 0;

  	while ((pcontext->output->buffer_free () < length) && pcontext->playing == TRUE)
  		usleep(5000);

	pcontext->pass_audio(pcontext,FMT_S16_LE,2 , length , buf , &pcontext->playing );

}
uint8_t buffer[131072];
uint32_t buffersize = 0;

void AddBuffer3(unsigned char *buf, unsigned int length) {
	int32_t i = 0;
	if(!AudioFirst) {
		AudioFirst = 1;


	}

	for(i = 0; i < (length >> 1); i+=2) {
		int32_t r = ((short*)buf)[i];
		int32_t l = ((short*)buf)[i + 1];

		r = (r * rel_volume) >> 8;
		l = (l * rel_volume) >> 8;

		((short*)buffer)[(buffersize>>1) + i + 1] = r;
		((short*)buffer)[(buffersize>>1) + i] = l;
	}

	buffersize+=length;

	if(buffersize > (32768-length)) {

		//writeAudio(hWaveOut,buf, length);
		//play_time += ((double)(buffersize >> 2) / (double)SampleRate);
		pcontext->playing = 1;
  		pcontext->eof = 0;
		pcontext->pass_audio(pcontext,FMT_S16_LE,2 , buffersize , buffer , &pcontext->playing );

		buffersize = 0;

	}

}



void AiLenChanged(void) {
	int32_t length = 0;
	uint32_t address = (AI_DRAM_ADDR_REG & 0x00FFFFF8);

    /*while(is_paused) {
		if(_kbhit()) {
			int key = _getch();
			kbcallback(key);
		}
		Sleep(1);
	}

	if(_kbhit()) {
		int key = _getch();
		kbcallback(key);
	}*/

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
