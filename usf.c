
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
#include <audacious/strings.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

extern int SampleRate;

extern InputPlugin usf_ip;
InputPlayback * pcontext = 0;
GThread * decode_thread = 0;

int8_t filename[512];
uint32_t cpu_running = 0, use_interpreter = 0, use_audiohle = 0, is_paused = 0, rel_volume = 256, cpu_stopped = 1;

uint32_t enablecompare = 0, enableFIFOfull = 0;

uint32_t usf_length = 0, usf_fade_length = 0;

int8_t title[100];
uint8_t title_format[] = "%game% - %title%";

extern int32_t RSP_Cpu;

uint32_t get_length_from_string(uint8_t * str_length) {
	uint32_t ttime = 0, temp = 0, mult = 1;
	uint8_t * src = str_length + strlen(str_length) - 1;

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
            	ttime += (temp * 1000);
            	temp = 0;
            }
		}
		src--;
    }
    ttime += (temp * 60000);
    return ttime;
}


void format_title(uint8_t * tags) {
	uint8_t * dst = title, * src = title_format;
	uint8_t tag_name[64], tag_buffer[50001], *tag = 0;
	uint32_t valid_title = 0;

	while(*src) {
		 if(*src == '%') {
		 	if(!tag) {
		 		tag = tag_name;
		 	} else {
		 		*tag = 0;

		 		psftag_raw_getvar(tags,tag_name,tag_buffer,50000);

		 		if(*tag_buffer) {

		 			memcpy(dst, tag_buffer, strlen(tag_buffer));
		 			dst += strlen(tag_buffer);

		 			if(!strcmp(tag_name,"title"))
		 				valid_title = 1;
		 		}
            	tag = 0;
		 	}

		 } else {
		 	if(tag)
		 		*(tag++) = *src;
		 	else
		 		*(dst++) = *src;
		 }
		src++;
	}

	*dst = 0;

	if(!valid_title)
		title[0] = 0;
}

int LoadUSF(char *fn) {
	FILE *fil = NULL;
	uint32_t reservedsize = 0, codesize = 0, crc = 0, tagstart = 0, reservestart = 0, filesize = 0, tagsize = 0, temp = 0;
	uint8_t buffer[16], *buffer2 = NULL, *tagbuffer = NULL;

	fil = fopen(fn, "rb");
	if(!fil) {
		printf("Could not open USF!\n");
		return 0;
	}

	fread(buffer,4 ,1 ,fil);
	if(buffer[0] != 'P' && buffer[1] != 'S' && buffer[2] != 'F' && buffer[3] != 0x21) {
		printf("Invalid header in file!\n");
		fclose(fil);
		return 0;
	}

    fread(&reservedsize, 4, 1, fil);
    fread(&codesize, 4, 1, fil);
    fread(&crc, 4, 1, fil);

    fseek(fil, 0, SEEK_END);
    filesize = ftell(fil);

    reservestart = 0x10;
    tagstart = reservestart + reservedsize;
    tagsize = filesize - tagstart;

	if(tagsize) {
		fseek(fil, tagstart, SEEK_SET);
		fread(buffer, 5, 1, fil);

		if(buffer[0] != '[' && buffer[1] != 'T' && buffer[2] != 'A' && buffer[3] != 'G' && buffer[4] != ']') {
			printf("Errornous data in tag area! %ld\n", tagsize);
			fclose(fil);
			return 0;
		}

		buffer2 = malloc(50001);
		tagbuffer = malloc(tagsize);

    	fread(tagbuffer, tagsize, 1, fil);

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

		format_title(tagbuffer);

        psftag_raw_getvar(tagbuffer,"length",buffer2,50000);
        if(strlen(buffer2))
        	usf_length = get_length_from_string(buffer2);

		free(buffer2);
		buffer2 = NULL;

		free(tagbuffer);
		tagbuffer = NULL;

	}

	fseek(fil, reservestart, SEEK_SET);
	fread(&temp, 4, 1, fil);

	if(temp == 0x34365253) { //there is a rom section
		int len = 0, start = 0;
		fread(&len, 4, 1, fil);

		while(len) {
			fread(&start, 4, 1, fil);

			while(len) {
				int page = start >> 16;
				int readLen = ( ((start + len) >> 16) > page) ? (((page + 1) << 16) - start) : len;

                if(ROMPages[page] == 0) {
                	ROMPages[page] = malloc(0x10000);
                	memset(ROMPages[page], 0, 0x10000);
                }

				fread(ROMPages[page] + (start & 0xffff), readLen, 1, fil);

				start += readLen;
				len -= readLen;
			}

			fread(&len, 4, 1, fil);
		}

	}

	fread(&temp, 4, 1, fil);
	if(temp == 0x34365253) {
		int len = 0, start = 0;
		fread(&len, 4, 1, fil);

		while(len) {
			fread(&start, 4, 1, fil);

			fread(savestatespace + start, len, 1, fil);

			fread(&len, 4, 1, fil);
		}

		//if(((SaveState*)STATE)->RamSize == 0x400000) {
		//	savestatespace = realloc(savestatespace, 0x40275c);
		//}

	}

    fclose(fil);

	return 1;
}


void usf_init()
{
	use_audiohle = 0;
	use_interpreter = 0;
	RSP_Cpu = 0;
}

void usf_about()
{
  //LoadSettings(&settings);
}

void usf_configure()
{
  //LoadSettings(&settings);
}

void usf_destroy()
{

}

void seek(int time_in_ms)
{

}

void usf_mseek(InputPlayback *data,gulong ms)
{

}

void usf_seek(InputPlayback *context,gint time)
{

}

void usf_file_info_box(gchar *pFile)
{

}

int usf_get_time(InputPlayback *context)
{
	if(!context->output->buffer_playing())
		return -1;
	return context->output->output_time();
}

void usf_play(InputPlayback *context)
{
	if(context->filename) {
		uint8_t * src = &context->filename[7], *dst = filename;
		while(*src) {
			if(*src != '%') {
				*dst = *src;
				dst++;
				src++;
			} else {
				uint8_t hex = 0;
				uint8_t hex_h = tolower(*(src + 1));
				uint8_t hex_l = tolower(*(src + 2));

				if(hex_h <= '9')
					hex = (hex_h - '0') * 0x10;
				else /*assume in the range A-F*/
					hex = ((hex_h - 'a') + 0xA) * 0x10;

				if(hex_l <= '9')
					hex += (hex_l - '0');
				else
					hex += ((hex_l - 'a') + 0xA);

                *dst = hex;
                dst++;
				src += 3;
			}
		}
		*dst = 0;
	}

	pcontext = context;
	decode_thread = g_thread_self();
    context->set_pb_ready(context);

    if(!Allocate_Memory()) {
		printf("Failed whilst allocating memory :*(\n");
		return 0;
	}

    if(!LoadUSF(filename)) {
    	Release_Memory();
    	return 0;
    }

    context->set_params(context,title,usf_length,SampleRate*4,SampleRate,2);

    cpu_running = 1;

    StartEmulationFromSave(savestatespace);

	cpu_running = 0;
}

void usf_stop(InputPlayback *context)
{
	CloseCpu();
	Release_Memory();
	g_thread_join(decode_thread);
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

static char *tt = "sample title";

static void usf_get_song_info(gchar *pFile,char **atitle,int *length)
{
	*atitle = g_strdup(tt);
	*length = 123456;
	printf("getting song info '%s'\n", atitle);
}

void usf_pause(InputPlayback *context,gshort paused)
{
}


gchar *usf_exts [] = {
  "usf",
  "miniusf",
  NULL
};

gint get_volume (gint * l, gint * r) { return 0;}
gint set_volume (gint l, gint r) { return 0;}

static Tuple *usf_get_song_tuple(gchar *fn) {
	Tuple *	tuple = NULL;
	tuple = aud_tuple_new_from_filename(fn);
	aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, 12345);
	//aud_tuple_associate_string(tuple, -1, "game", "game");
	aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, "title");
	return tuple;
}

/*

static Tuple *get_aud_tuple_psf(gchar *fn) {
    Tuple *tuple = NULL;
    PSFINFO *tmp = sexypsf_getpsfinfo(fn);

    if (tmp->length) {
        tuple = aud_tuple_new_from_filename(fn);
        aud_tuple_associate_int(tuple, FIELD_LENGTH, NULL, tmp->length);
        aud_tuple_associate_string(tuple, FIELD_ARTIST, NULL, tmp->artist);
        aud_tuple_associate_string(tuple, FIELD_ALBUM, NULL, tmp->game);
        aud_tuple_associate_string(tuple, -1, "game", tmp->game);
        aud_tuple_associate_string(tuple, FIELD_TITLE, NULL, tmp->title);
        aud_tuple_associate_string(tuple, FIELD_GENRE, NULL, tmp->genre);
        aud_tuple_associate_string(tuple, FIELD_COPYRIGHT, NULL, tmp->copyright);
*/


InputPlugin usf_ip = {
  .description = (gchar *)"LazyUSF Decoder",
  .init = usf_init,
  .about = usf_about,
  .configure = usf_configure,
  .cleanup = usf_destroy,
  .is_our_file = usf_is_our_file,
  .play_file = usf_play,
  .stop = usf_stop,
  .pause = usf_pause,
  .seek = usf_seek,
  .get_time = usf_get_time,
  .get_song_info = usf_get_song_info,
  .vfs_extensions = usf_exts,
  .mseek = usf_mseek,
  .file_info_box = usf_file_info_box,
  /*.get_song_tuple = usf_get_song_tuple,*/
};



static InputPlugin *usf_iplist[] = { &usf_ip, NULL };

DECLARE_PLUGIN(usf_iplist, NULL, NULL, usf_iplist, NULL, NULL, NULL, NULL, NULL);

