/* Test elementary reading and writing that libaudio does:
 * 1. Generate a sine wave of given freq, rate, wlen.
 * 2. Write the raw samples into a file.
 * 3. Read the raw samples back.
 * 4. Check that they are the same, byte by byte.
 * 5. Repeat for every encoding we support.
 * 6. Return 0 iff there was no error.
 *
 * FIXME beware the sin() of a large argument (fmod?)
 * FIXME multichanel
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <err.h>

#include "audio.h"

struct encoding {
	uint32_t	encoding;
	char		name[256];
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
#define NUMENCODING (sizeof(encodings) / sizeof(struct encoding))

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
		(*wave)[i] = sin(2 * M_PI * fmod(freq * t, 1.0));
}

/* A sound wave is given as an array of floats.
 * Write it into a file of the given encoding, read it back,
 * and check that the samples are (reasonably) equal.
 * For a format with less than 32 bits, there will be a loss of precision.
 * For example, writing the f32 sample as s16le and reading the s16le back
 * as a float will not be the same float. On the other hand, any 32bit format
 * should reconstruct the sample precisely. */
/* FIXME: do it for < 32 formats too if -v is given
 * FIXME: test the < 32 formats for "enough" precision"
 * FIXME: have a separate test-precision.c for this? */
int
testrw(struct encoding *e, const float* wave, const size_t wlen, const int rate)
{
	char name[FILENAME_MAX];
	AUINFO *info = NULL;
	AUFILE *file = NULL;
	ssize_t i, r, w;
	float *rbuf;

	if (e == NULL)
		return 1;

	if ((info = calloc(1, sizeof(AUINFO))) == NULL)
		err(1, NULL);
	info->channels = 1;
	info->srate    = rate;
	info->encoding = e->encoding;

	snprintf(name, FILENAME_MAX, "%s.raw", e->name);
	if ((file = au_open(name, AU_WRITE, info)) == NULL) {
		warnx("Cannot open %s for writing", name);
		return 1;
	}
	/*au_info(file);*/

	if ((w = au_write_f32(file, wave, wlen)) == -1) {
		warnx("Cannot write to %s", name);
		return 1;
	} else if (w < wlen) {
		warnx("Only wrote %zd < %zd samples", w, wlen);
		return 1;
	}
	au_close(file);

	if ((rbuf = calloc(wlen, sizeof(float))) == NULL)
		err(1, NULL);
	if ((file = au_open(name, AU_READ, info)) == NULL) {
		warnx("Cannot open %s for reading", name);
		return 1;
	}
	if ((r = au_read_f32(file, rbuf, wlen)) == -1) {
		warnx("Cannot read from to %s", name);
		return 1;
	} else if (r < wlen) {
		warnx("Only read %zd < %zd samples", r, w);
		return 1;
	}
	au_close(file);

	printf("%s\n", name);
	for (i = 0; i < r; i++) {
		printf("%9zd % 10.8f % 10.8f\n", i, wave[i], rbuf[i]);
		/* FIXME: For 32 bit formats, test that the sample is the same.
		 * For < 32 bit formats, test that it is reasonably close. */
	}

	return 0;
}

int
main(int argc, char** argv)
{
	int rate = 4000;
	int freq = 213;
	int wlen = 1;
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
