#include <unistd.h>
#include <err.h>

#include "audio.h"
#include "wav.h"

/* Read a WAV header from an open file and fill AUINFO accordingly.
 * This is only done during au_open() so we don't seek there and back.
 * Return 0 for success, -1 on error. */
int
wav_read_hdr(int fd, AUINFO* info)
{
	struct wavhdr hdr;
	if (NULL == info)
		return -1;
	/* FIXME */
	read(fd, &hdr, sizeof(struct wavhdr));
	return 0;
}

/* Write a WAV header at the start of an open file as per AUINFO.
 * Seek there and back, so that subsequent samples are written correctly.
 * Return 0 for success, -1 on error. */
int
wav_write_hdr(int fd, AUINFO* info)
{
	off_t pos;
	struct wavhdr hdr;
	if (NULL == info)
		return -1;
	if (-1 == (pos = lseek(fd, 0, SEEK_CUR)))
		return -1;
	if (-1 == lseek(fd, 0, SEEK_SET))
		return -1;
	/* FIXME */
	read(fd, &hdr, sizeof(struct wavhdr));
	if (-1 == lseek(fd, pos, SEEK_SET))
		return -1;
	return 0;
}

int
wav_init(AUFILE *file)
{
	if (file == NULL || file->info == NULL)
		return -1;
	if (file->info->filetype != AU_FILETYPE_WAV) {
		warnx("Will not intitialize non WAV file as WAV");
		return -1;
	}
	file->au_read_hdr = wav_read_hdr;
	file->au_write_hdr = wav_write_hdr;
	return 0;
}
