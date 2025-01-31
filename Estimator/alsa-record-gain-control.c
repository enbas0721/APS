/*
   From on Paul David's tutorial : http://equalarea.com/paul/alsa-audio.html
   Fixes rate and buffer problems
   sudo apt-get install libasound2-dev
   gcc -o gain.out alsa-record-gain-control.c WavManager/audioio.c -lasound
   ./gain.out hw:1
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include "WavManager/audioio.h"

#define SMPL 44100
#define BIT 16

static int smixer_level = 0;
static struct snd_mixer_selem_regopt smixer_options;

static int set_gain_value(long value, char card)
{
	int err;

	static snd_mixer_t *handle = NULL;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);

	snd_mixer_selem_channel_id_t chn = SND_MIXER_SCHN_FRONT_LEFT;

	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, "Mic");

	if (handle == NULL) {
		// snd_mixerのオープン
		if ((err = snd_mixer_open(&handle, 0)) < 0) {
			fprintf(stderr, "Mixer %s open error\n", card);
			return err;
		}
		// snd_mixer_attach
		if (smixer_level == 0 && (err = snd_mixer_attach(handle, card)) < 0) {
			fprintf(stderr, "Mixer attach %s error\n", card);
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
		if ((err = snd_mixer_selem_register(handle, smixer_level > 0 ? &smixer_options : NULL, NULL)) < 0) {
			fprintf(stderr, "Mixer register error\n");
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
		err = snd_mixer_load(handle);
		if (err < 0) {
			fprintf(stderr, "Mixer %s load error", card);
			snd_mixer_close(handle);
			handle = NULL;
			return err;
		}
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		fprintf(stderr, "Unable to find simple control '%s',%i\n", snd_mixer_selem_id_get_name(sid), snd_mixer_selem_id_get_index(sid));
		snd_mixer_close(handle);
		handle = NULL;
	}

	err = snd_mixer_selem_set_capture_volume(elem, chn, value);
	if (err < 0) {
		fprintf(stderr, "Setting %s capture volume error", card);
		snd_mixer_close(handle);
		handle = NULL;
		return err;
	}

	// 今後も使うかによる
	snd_mixer_close(handle);
	handle = NULL;

	return err < 0 ? 1 : 0;
}

int main (int argc, char *argv[])
{
	// バッファ系の変数
	int i;
	int err;
	int16_t *buffer;
	int buffer_frames = 1024;
	unsigned int rate = SMPL;
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

	char card[8];
	for (size_t i = 0; i < sizeof(argv[1]); i++) {
		card[i] = argv[1][i];
	}

	double recording_time = 10.0f;

	// Wavファイル作成用
	WAV_PRM prm;
	int16_t *record_data;
	char filename[64] = "output.wav";

	// パラメータコピー
	prm.fs = SMPL;
	prm.bits = BIT;
	prm.L =  prm.fs * recording_time;

	if ((err = snd_pcm_open (&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stdout, "cannot open audio device %s (%s)\n",
		         argv[1],
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "audio interface opened\n");

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stdout, "cannot allocate hardware parameter structure (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params allocated\n");

	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stdout, "cannot initialize hardware parameter structure (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params initialized\n");

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stdout, "cannot set access type (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params access setted\n");

	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) {
		fprintf (stdout, "cannot set sample format (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params format setted\n");

	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) {
		fprintf (stdout, "cannot set sample rate (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params rate setted\n");

	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 1)) < 0) {
		fprintf (stdout, "cannot set channel count (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params channels setted\n");

	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stdout, "cannot set parameters (%s)\n",
		         snd_strerror (err));
		exit (1);
	}

	fprintf(stdout, "hw_params setted\n");

	snd_pcm_hw_params_free (hw_params);

	fprintf(stdout, "hw_params freed\n");

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stdout, "cannot prepare audio interface for use (%s)\n",
		         snd_strerror (err));
		exit (1);
	}
	fprintf(stdout, "audio interface prepared\n");

	record_data = calloc(prm.L, sizeof(int16_t));
	buffer = (int16_t*)malloc(sizeof(int16_t)*buffer_frames*snd_pcm_format_width(format));

	fprintf(stdout, "buffer allocated\n");

	int gain_value = 16;
	set_gain_value(gain_value, card);
	int flag = 0;
	int current_index = 0;
	while ((current_index + buffer_frames) < prm.L) {
		if ((err = snd_pcm_readi(capture_handle, (void*)buffer, buffer_frames)) != buffer_frames) {
			fprintf(stdout, "read from audio interface failed (%s)\n",err, snd_strerror(err));
			exit (1);
		}
		if (flag == 0) {
			if (current_index >= (prm.L/3)) {
				flag += 1;
				gain_value -= 5;
				printf("gain_changed\n");
				set_gain_value(gain_value, card);
			}
		}else if(flag == 1) {
			if (current_index >= (2*prm.L/3)) {
				flag += 1;
				gain_value -= 5;
				printf("gain_changed\n");
				set_gain_value(gain_value, card);
			}
		}
		for (int i = current_index; i < current_index + err; i++) {
			record_data[i] = buffer[i-current_index];
		}
		fprintf(stdout, "Recorded: %d/%d\n",current_index, prm.L);
		current_index = current_index + err;
	}

	fprintf(stdout, "Record data final 5: ");
	for(int j = current_index - 5; j < current_index; j++) {
		fprintf(stdout, "%d ", record_data[j]);
	}
	printf("\n");
	audio_write(record_data, &prm, filename);

	free(buffer);
	free(record_data);

	fprintf(stdout, "buffer freed\n");
	snd_pcm_close(capture_handle);
	fprintf(stdout, "audio interface closed\n");

	exit (0);
	return 0;
}
