
#include <stdint.h>
#include "usf.h"
#include "cpu.h"
#include "memory.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>

#include <audacious/util.h>
#include <audacious/configdb.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

extern int SampleRate;

extern InputPlugin usf_ip;
InputPlayback * pcontext = 0;
GThread * decode_thread = 0;

int8_t filename[512];
uint32_t cpu_running = 0, use_interpreter = 0, use_audiohle = 0, is_paused = 0, cpu_stopped = 1;
uint32_t is_fading = 0, fade_type = 1, fade_time = 5000, is_seeking = 0, seek_backwards = 0, track_time = 180000;
double seek_time = 0.0, play_time = 0.0, rel_volume = 1.0;

uint32_t enablecompare = 0, enableFIFOfull = 0;

uint32_t usf_length = 0, usf_fade_length = 0;

int8_t title[100];
uint8_t title_format[] = "%game% - %title%";

extern int32_t RSP_Cpu;

uint32_t get_length_from_string(uint8_t * str_length) {
	uint32_t ttime = 0, temp = 0, mult = 1, level = 1;
	char Source[1024];
	uint8_t * src = Source + strlen(str_length);
	strcpy(&Source[1], str_length);
	Source[0] = 0;

    while(*src) {
		if((*src >= '0') && (*src <= '9')) {
			temp += ((*src - '0') * mult);
			mult *= 10;
		} else {
			mult = 1;
            if(*src == '.') {
            	ttime = temp;
            	temp = 0;
            } else if(*src  == ':') {
            	ttime += (temp * (1000 * level));
            	temp = 0;
				level *= 60;
            }
		}
		src--;
    }

    ttime += (temp * (1000 * level));
    return ttime;
}

int LoadUSF(const gchar * fn)
{
	VFSFile * fil = NULL;
	uint32_t reservedsize = 0, codesize = 0, crc = 0, tagstart = 0, reservestart = 0, filesize = 0, tagsize = 0, temp = 0;
	uint8_t buffer[16], * buffer2 = NULL, * tagbuffer = NULL;

	is_fading = 0;
	fade_type = 1;
	fade_time = 5000;
	track_time = 180000;
	play_time = 0;
	is_seeking = 0;
	seek_backwards = 0;
	seek_time = 0;

	fil = aud_vfs_fopen(fn, "rb");

	if(!fil) {
		printf("Could not open USF!\n");
		return 0;
	}

	aud_vfs_fread(buffer,4 ,1 ,fil);
	if(buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21) {
		printf("Invalid header in file!\n");
		aud_vfs_fclose(fil);
		return 0;
	}

    aud_vfs_fread(&reservedsize, 4, 1, fil);
    aud_vfs_fread(&codesize, 4, 1, fil);
    aud_vfs_fread(&crc, 4, 1, fil);

    aud_vfs_fseek(fil, 0, SEEK_END);
    filesize = aud_vfs_ftell(fil);

    reservestart = 0x10;
    tagstart = reservestart + reservedsize;
    tagsize = filesize - tagstart;

	if(tagsize) {
		aud_vfs_fseek(fil, tagstart, SEEK_SET);
		aud_vfs_fread(buffer, 5, 1, fil);

		if(buffer[0] != '[' && buffer[1] != 'T' && buffer[2] != 'A' && buffer[3] != 'G' && buffer[4] != ']') {
			printf("Errornous data in tag area! %ld\n", tagsize);
			aud_vfs_fclose(fil);
			return 0;
		}

		buffer2 = malloc(50001);
		tagbuffer = malloc(tagsize);

    	aud_vfs_fread(tagbuffer, tagsize, 1, fil);

		psftag_raw_getvar(tagbuffer,"_lib",buffer2,50000);

		if(strlen(buffer2)) {
			char path[512];
			int pathlength = 0;

			if(strrchr(fn, '/')) //linux
				pathlength = strrchr(fn, '/') - fn + 1;
			else if(strrchr(fn, '\\')) //windows
				pathlength = strrchr(fn, '\\') - fn + 1;
			else //no path
				pathlength = strlen(fn);

			strncpy(path, fn, pathlength);
			path[pathlength] = 0;
			strcat(path, buffer2);

			LoadUSF(path);
		}

		psftag_raw_getvar(tagbuffer,"_enablecompare",buffer2,50000);
		if(strlen(buffer2))
			enablecompare = 1;
		else
			enablecompare = 0;

		psftag_raw_getvar(tagbuffer,"_enableFIFOfull",buffer2,50000);
		if(strlen(buffer2))
			enableFIFOfull = 1;
		else
			enableFIFOfull = 0;

		psftag_raw_getvar(tagbuffer, "length", buffer2, 50000);
        if(strlen(buffer2)) {
			track_time = get_length_from_string(buffer2);
		}

		psftag_raw_getvar(tagbuffer, "fade", buffer2, 50000);
        if(strlen(buffer2)) {
			fade_time = get_length_from_string(buffer2);
		}


		free(buffer2);
		buffer2 = NULL;

		free(tagbuffer);
		tagbuffer = NULL;

	}

	aud_vfs_fseek(fil, reservestart, SEEK_SET);
	aud_vfs_fread(&temp, 4, 1, fil);

	if(temp == 0x34365253) { //there is a rom section
		int len = 0, start = 0;
		aud_vfs_fread(&len, 4, 1, fil);

		while(len) {
			aud_vfs_fread(&start, 4, 1, fil);

			while(len) {
				int page = start >> 16;
				int readLen = ( ((start + len) >> 16) > page) ? (((page + 1) << 16) - start) : len;

                if(ROMPages[page] == 0) {
                	ROMPages[page] = malloc(0x10000);
                	memset(ROMPages[page], 0, 0x10000);
                }

				aud_vfs_fread(ROMPages[page] + (start & 0xffff), readLen, 1, fil);

				start += readLen;
				len -= readLen;
			}

			aud_vfs_fread(&len, 4, 1, fil);
		}

	}

	aud_vfs_fread(&temp, 4, 1, fil);
	if(temp == 0x34365253) {
		int len = 0, start = 0;
		aud_vfs_fread(&len, 4, 1, fil);

		while(len) {
			aud_vfs_fread(&start, 4, 1, fil);

			aud_vfs_fread(savestatespace + start, len, 1, fil);

			aud_vfs_fread(&len, 4, 1, fil);
		}
	}

    aud_vfs_fclose(fil);

	return 1;
}


void usf_init()
{
	use_audiohle = 0;
	use_interpreter = 0;
	RSP_Cpu = 0; // 0 is recompiler, 1 is interpreter
}

void usf_destroy()
{

}

void usf_seek(InputPlayback * context, gint time)
{
	is_seeking = 1;
	seek_time = time * 1000.0;
	context->output->flush(time);
}


void usf_mseek(InputPlayback * context, gulong millisecond)
{
	is_seeking = 1;
	seek_time = (double)millisecond;
	context->output->flush(millisecond/1000);
}

void usf_play(InputPlayback * context)
{
	if(!context->filename)
		return;

	// Defaults (which would be overriden by Tags / playing
	savestatespace = NULL;
	cpu_running = is_paused = 0;
	cpu_stopped = 1;
	is_fading = 0;
	fade_type = 1;
	fade_time = 5000;
	is_seeking = 0;
	seek_backwards = 0;
	track_time = 180000;
	seek_time = 0.0;
	play_time = 0.0;
	rel_volume = 1.0;


	pcontext = context;
	decode_thread = g_thread_self();
    context->set_pb_ready(context);

    if(!Allocate_Memory()) {
		printf("Failed whilst allocating memory :*(\n");
		return 0;
	}

    if(!LoadUSF(context->filename)) {
		Release_Memory();
    	return 0;
    }

 //   context->set_params(context,title,usf_length,-1,SampleRate,2);

    StartEmulationFromSave(savestatespace);


	//Release_Memory();
}

void usf_stop(InputPlayback *context)
{

	if(!cpu_running)
		return;

	CloseCpu();
	g_thread_join(decode_thread);

	Release_Memory();

	context->output->close_audio();
}

gboolean usf_is_our_file(char *pFile)
{
  const char *pExt;
  gchar **exts;

  if (!pFile)
    return FALSE;

  /* get extension */
  pExt = strrchr(pFile,'.');
  if (!pExt)
    return FALSE;
  /* skip past period */
  ++pExt;

  if ((strcasecmp(pExt,"usf") == 0) ||
  	  (strcasecmp(pExt,"miniusf") == 0))
  	  return TRUE;

  return FALSE;
}

void usf_pause(InputPlayback *context, gshort paused)
{
}

const gchar *usf_exts [] =
{
  "usf",
  "miniusf",
  NULL
};


static Tuple * usf_get_song_tuple(const gchar * fn)
{
	Tuple *	tuple = NULL;

	VFSFile * fil = NULL;
	uint32_t reservedsize = 0, codesize = 0, crc = 0, tagstart = 0, reservestart = 0, filesize = 0, tagsize = 0, temp = 0;
	uint8_t buffer[16], * buffer2 = NULL, * tagbuffer = NULL;

	fil = aud_vfs_fopen(fn, "rb");

	if(!fil) {
		printf("Could not open USF!\n");
		return NULL;
	}

	aud_vfs_fread(buffer,4 ,1 ,fil);

	if(buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21) {
		printf("Invalid header in file!\n");
		aud_vfs_fclose(fil);
		return NULL;
	}

    aud_vfs_fread(&reservedsize, 4, 1, fil);
    aud_vfs_fread(&codesize, 4, 1, fil);
    aud_vfs_fread(&crc, 4, 1, fil);

    aud_vfs_fseek(fil, 0, SEEK_END);
    filesize = aud_vfs_ftell(fil);

    reservestart = 0x10;
    tagstart = reservestart + reservedsize;
    tagsize = filesize - tagstart;

	tuple = aud_tuple_new_from_filename(fn);

	if(tagsize) {
		int temp_fade = 0;
		aud_vfs_fseek(fil, tagstart, SEEK_SET);
		aud_vfs_fread(buffer, 5, 1, fil);

		if(buffer[0] != '[' && buffer[1] != 'T' && buffer[2] != 'A' && buffer[3] != 'G' && buffer[4] != ']') {
			printf("Errornous data in tag area! %ld\n", tagsize);
			aud_vfs_fclose(fil);
			return NULL;
		}

		buffer2 = malloc(50001);
		tagbuffer = malloc(tagsize);

    	aud_vfs_fread(tagbuffer, tagsize, 1, fil);

		psftag_raw_getvar(tagbuffer, "fade", buffer2, 50000);
        if(strlen(buffer2))
			temp_fade = get_length_from_string(buffer2);

		psftag_raw_getvar(tagbuffer, "length", buffer2, 50000);
        if(strlen(buffer2))
        	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, get_length_from_string(buffer2) + temp_fade);
		else
			aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, (180*1000));

		psftag_raw_getvar(tagbuffer, "title", buffer2, 50000);
        if(strlen(buffer2))
			aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, buffer2);
		else
		{
			char title[512];
			int pathlength = 0;

			if(strrchr(fn, '/')) //linux
				pathlength = strrchr(fn, '/') - fn + 1;
			else if(strrchr(fn, '\\')) //windows
				pathlength = strrchr(fn, '\\') - fn + 1;
			else //no path
				pathlength = 7;

			strcpy(title, &fn[pathlength]);

			aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, title);

		}

		psftag_raw_getvar(tagbuffer, "artist", buffer2, 50000);
        if(strlen(buffer2))
			aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, buffer2);

		psftag_raw_getvar(tagbuffer, "game", buffer2, 50000);
        if(strlen(buffer2)) {
			aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, buffer2);
			aud_tuple_associate_string(tuple, -1, "game", buffer2);
		}

		psftag_raw_getvar(tagbuffer, "copyright", buffer2, 50000);
        if(strlen(buffer2))
			aud_tuple_associate_string(tuple, FIELD_COPYRIGHT, NULL, buffer2);

		aud_tuple_associate_string(tuple, FIELD_QUALITY, NULL, "sequenced");

		aud_tuple_associate_string(tuple, FIELD_CODEC, NULL, "Nintendo 64 Audio");
	}
	else
	{
		char title[512];
		int pathlength = 0;

		if(strrchr(fn, '/')) //linux
			pathlength = strrchr(fn, '/') - fn + 1;
		else if(strrchr(fn, '\\')) //windows
			pathlength = strrchr(fn, '\\') - fn + 1;
		else //no path
			pathlength = 7;

		strcpy(title, &fn[pathlength]);


		aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, (180 * 1000));
		aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, title);
	}

	return tuple;
}

int usf_get_time(InputPlayback * playback)
{
	return (int)play_time;
}

InputPlugin usf_ip = {
  .description = (gchar *)"LazyUSF Decoder",
  .init = usf_init,
  .cleanup = usf_destroy,
  .is_our_file = usf_is_our_file,
  .play_file = usf_play,
  .stop = usf_stop,
  .pause = usf_pause,
  .seek = usf_seek,
  .mseek = usf_mseek,
  .vfs_extensions = usf_exts,
  .get_song_tuple = usf_get_song_tuple,
  .get_time = usf_get_time,
};



static InputPlugin *usf_iplist[] = { &usf_ip, NULL };

DECLARE_PLUGIN(usf_iplist, NULL, NULL, usf_iplist, NULL, NULL, NULL, NULL, NULL);

