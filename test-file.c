#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <err.h>

#include "audio.h"
#define NAME "test-file.raw"

int
main(int argc, char** argv)
{
	AUINFO info;
	AUFILE *file = NULL;

	bzero(&info, sizeof(info));
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.srate = 48000;
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.encoding = AU_ENCTYPE_PCM;
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.encoding |= AU_ENCODING_FLOAT;
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.encoding |= AU_ORDER_LE;
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.encoding |= 32;
	if ((file = au_open(NAME, AU_WRITE, &info)))
		return 1;

	info.channels = 1;
	if ((file = au_open(NAME, AU_WRITE, &info)) == NULL)
		return 1;

	if (au_close(file))
		return 1;

	bzero(&info, sizeof(info));
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.srate = 48000;
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.encoding = AU_ENCTYPE_PCM;
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.encoding |= AU_ENCODING_FLOAT;
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.encoding |= AU_ORDER_LE;
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.encoding |= 32;
	if ((file = au_open(NAME, AU_READ, &info)))
		return 1;

	info.channels = 1;
	if ((file = au_open(NAME, AU_READ, &info)) == NULL)
		return 1;

	if (au_close(file))
		return 1;

	return 0;
}
