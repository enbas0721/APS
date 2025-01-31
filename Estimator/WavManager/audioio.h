#ifndef AUDIOIO_H
#define AUDIOIO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct
{
	int fs;
	int bits;
	int L;
}WAV_PRM;

int16_t *audio_read(WAV_PRM *prm, char *filename);
void audio_write(int16_t *data, WAV_PRM *prm, char *filename);

#endif
