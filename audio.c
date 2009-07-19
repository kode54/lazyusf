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

static uint32_t ResampleRate = 44100;
int32_t ResampleOn = 0, UseResampler = 1;

static int32_t samplebuf[65536];
static int16_t histl[4], histr[4], hp = 0;

void OpenSound(void)
{

	hp = 0;
	memset(&histl[0], 0, 8);
	memset(&histr[0], 0, 8);
  	/*if(fd != 0) {
  		printf("already open!\n");
  	}
  	fd = open("/dev/dsp", O_WRONLY);
	if (fd < 0) {
		perror("open of /dev/dsp failed");
		exit(1);
	}   */
	ResampleOn = 0;
	
	if (pcontext->output->open_audio(FMT_S16_NE,SampleRate,2) == 0) {
		if (UseResampler) {
			if(pcontext->output->open_audio(FMT_S16_NE,ResampleRate,2) == 0) {
			/*couldnt start the outputter*/
			cpu_running = 0;
			} else {
				printf("Using resampler\n");
				ResampleOn = 1;
			}
		}
	} else {
		printf("SUCCESS! - %d\n", SampleRate);
	}
}

// playback->pass_audio(playback,FMT_S16_LE,vgmstream->channels , l , buffer , &playback->playing );



void AddBuffer(unsigned char *buf, unsigned int length) {
	const int mask = ~((((16 / 8) * 2)) - 1);
	int32_t i = 0, ia = 0;
	uint32_t r;  
	pcontext->playing = 1;
  	pcontext->eof = 0;

	if(ResampleOn) {
		r = SampleRate * 0x1000;
		r /= ResampleRate;


		for(i = 0; i < ((length >> 2) << 12); ia++, i+=r) {
			uint32_t l =0, r = 0;
			
			histr[hp] = (int16_t)(((uint32_t*)buf)[i>>12] >> 16);
			histl[hp] = ((int32_t*)buf)[i>>12] & 0xffff;

			l = (histl[0] + histl[1]) >> 1;
			r = (histr[0] + histr[1]) >> 1;

			((int16_t*)&samplebuf[ia])[0] = l;
			((int16_t*)&samplebuf[ia])[1] = r;

			//hp = (hp+1)&3;
			
			hp++;
			if(hp>1) hp = 0;
			
		}
	}


  	//if (t > length)
		//pcontext->pass_audio(pcontext,FMT_S16_LE,2 , length , buf , &pcontext->playing );
	//printf("%d\n", pcontext->output->buffer_free () );
	
	while ((pcontext->output->buffer_free () < (length))/* && pcontext->playing == TRUE*/)
  		g_usleep(20000);


	//while(pcontext->playing && pcontext->output->buffer_playing()) g_usleep(30);
	
	//pcontext->pass_audio(pcontext,FMT_S16_LE,2 , length , buf , &pcontext->playing );
	if(ResampleOn)
		pcontext->pass_audio(pcontext,FMT_S16_NE,2 , ia<<2 , samplebuf , &pcontext->playing );
	else
		pcontext->pass_audio(pcontext,FMT_S16_NE,2 , length , buf , &pcontext->playing );

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
