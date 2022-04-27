/* Test elementary reading and writing that libaudio does:
 * 1. Generate a sine wave of given frequency, rate and length.
 * 2. Write the raw samples into a file and read them back.
 * 4. Create an audio file containing the difference.
 * 5. Repeat for every encoding we support.
 * 6. Return 0 iff there was no error.
 *
 * FIXME beware the sin() of a large argument (fmod?)
 * FIXME multichanel? Or should that be tested separately?
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <err.h>

#include "audio.h"

struct encoding {
	uint32_t	encoding;
	char		name[32];
} encodings[] = {
{ AU_ENCTYPE_PCM | AU_ENCODING_SIGNED   | AU_ORDER_NONE |  8, "pcm-s08"   },
{ AU_ENCTYPE_PCM | AU_ENCODING_UNSIGNED | AU_ORDER_NONE |  8, "pcm-u08"   },
{ AU_ENCTYPE_PCM | AU_ENCODING_SIGNED   | AU_ORDER_LE   | 16, "pcm-s16le" },
{ AU_ENCTYPE_PCM | AU_ENCODING_SIGNED   | AU_ORDER_BE   | 16, "pcm-s16be" },
{ AU_ENCTYPE_PCM | AU_ENCODING_UNSIGNED | AU_ORDER_LE   | 16, "pcm-u16le" },
{ AU_ENCTYPE_PCM | AU_ENCODING_UNSIGNED | AU_ORDER_BE   | 16, "pcm-u16be" },
{ AU_ENCTYPE_PCM | AU_ENCODING_SIGNED   | AU_ORDER_LE   | 32, "pcm-s32le" },
{ AU_ENCTYPE_PCM | AU_ENCODING_SIGNED   | AU_ORDER_BE   | 32, "pcm-s32be" },
{ AU_ENCTYPE_PCM | AU_ENCODING_UNSIGNED | AU_ORDER_LE   | 32, "pcm-u32le" },
{ AU_ENCTYPE_PCM | AU_ENCODING_UNSIGNED | AU_ORDER_BE   | 32, "pcm-u32be" },
{ AU_ENCTYPE_PCM | AU_ENCODING_FLOAT    | AU_ORDER_LE   | 32, "pcm-f32le" },
{ AU_ENCTYPE_PCM | AU_ENCODING_FLOAT    | AU_ORDER_BE   | 32, "pcm-f32be" }
};
#define NUMENCODING ((int)(sizeof(encodings) / sizeof(struct encoding)))

void
usage()
{
	warnx("usage: ./test-rw [-r rate] [-f freq] [-l wlen]");
}

void
genwave(ssize_t wlen, float **wave, int freq, int rate)
{
	int i;
	float t = 0, delta = 1.0 / rate;
	if ((*wave = calloc(wlen, sizeof(float))) == NULL)
		err(1, NULL);
	for (i = 0; i < wlen; t = ++i * delta)
		(*wave)[i] = .5 * M_SQRT2 * sin(2 * M_PI * fmod(freq * t, 1.0));
}

/* Write the given float sound wave into the given file.
 * Return number of samples written, or -1 on error. */
ssize_t
auwrite(const char* name, AUINFO* info, const float* samples, const ssize_t len)
{
	ssize_t w;
	AUFILE *file = NULL;
	if ((file = au_open(name, AU_WRITE, info)) == NULL) {
		warnx("Cannot open %s for writing", name);
		return -1;
	}
	if ((w = au_write_f32(file, samples, len)) == -1) {
		warnx("Cannot write to %s", name);
		return -1;
	} else if (w < len) {
		warnx("Only wrote %zd < %zd samples", w, len);
		return -1;
	}
	if (au_close(file))
		return -1;
	return w;
}

/* Read a sound wave from a given file as floats.
 * Return number of samples read, or -1 on error. */
ssize_t
auread(const char* name, AUINFO* info, float* samples, ssize_t len)
{
	ssize_t r;
	AUFILE *file = NULL;
	if ((file = au_open(name, AU_READ, info)) == NULL) {
		warnx("Cannot open %s for reading", name);
		return -1;
	}
	if ((r = au_read_f32(file, samples, len)) == -1) {
		warnx("Cannot read from %s", name);
		return -1;
	} else if (r < len) {
		warnx("Only read %zd < %zd samples", r, len);
		return -1;
	}
	if (au_close(file))
		return -1;
	return r;
}

int
testrw(struct encoding *e, const float* wave, const ssize_t len, const int rate)
{
	char name[FILENAME_MAX];
	AUINFO *info = NULL;
	ssize_t i, r, w;
	float *rbuf;
	float *diff;

	if (e == NULL)
		return 1;

	if ((info = calloc(1, sizeof(AUINFO))) == NULL)
		err(1, NULL);
	info->channels = 1;
	info->srate    = rate;
	info->encoding = e->encoding;

	/* Write the float wave using the given encoding. */
	snprintf(name, FILENAME_MAX, "%s.raw", e->name);
	if ((w = auwrite(name, info, wave, len)) == -1) {
		warnx("Error writing wave to %s", name);
		return 1;
	}

	/* Read the samples back as floats again. */
	if ((rbuf = calloc(len, sizeof(float))) == NULL)
		err(1, NULL);
	if ((r = auread(name, info, rbuf, w)) == -1) {
		warnx("Error reading back from %s", name);
		return 1;
	}

	/* For a format with < 32 bits, there will be a loss of precision;
	 * but any 32 bit format should reconstruct the sample precisely. */
	if ((diff = calloc(r, sizeof(float))) == NULL)
		err(1, NULL);
	for (i = 0; i < r; i++)
		diff[i] = wave[i] - rbuf[i];

	/* Write the audio diff file, using floats. */
	snprintf(name, FILENAME_MAX, "diff-%s.raw", e->name);
	info->encoding = AU_ENCTYPE_PCM | AU_ENCODING_FLOAT | AU_ORDER_LE | 32;
	if ((w = auwrite(name, info, diff, r)) == -1) {
		warnx("Error writing diff to %s", name);
		return 1;
	}

	free(rbuf);
	free(diff);
	return 0;
}

int
main(int argc, char** argv)
{
	int rate = 48000;	/* in Hertz   */
	int freq = 237;		/* in Hertz   */
	int wlen = 1;		/* in seconds */
	float *wave;
	int i, c;

	while ((c = getopt(argc, argv, "f:l:r:")) != -1) {
		switch (c) {
			case 'f':
				freq = atoi(optarg);
				break;
			case 'l':
				wlen = atoi(optarg);
				break;
			case 'r':
				rate = atoi(optarg);
				break;
			default:
				usage();
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (rate <= 0)
		errx(1, "-r rate needs to be a positive integer");
	if (freq <= 0)
		errx(1, "-f freq needs to be a positive integer");
	if (freq > rate/2)
		errx(1, "-f freq needs to be at most half the rate");
	if (wlen <= 0)
		errx(1, "-l wlen needs to be a positive integer");

	wlen *= rate;
	genwave(wlen, &wave, freq, rate);
	for (i = 0; i < NUMENCODING; i++)
		if (testrw(&encodings[i], wave, wlen, rate))
			return 1;
	return 0;
}
