#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include "audio.h"
#include "pcm.h"

struct {
	char	suff[8];
	char	name[64];
} filetypes[] = {
/* AU_FILETYPE_UNKNOWN	*/ { "",	""		},
/* AU_FILETYPE_RAW	*/ { "raw",	"raw audio"	},
/* AU_FILETYPE_WAV	*/ { "wav",	"wav audio"	},
};

AUFILETYPE
suff2type(const char* suff)
{
	int i;
	if (suff == NULL)
		return AU_FILETYPE_UNKNOWN;
	for (i = 1; i < NUMTYPES; i++)
		if (0 == strncasecmp(suff, filetypes[i].suff, 4))
			return i;
	return AU_FILETYPE_UNKNOWN;
}

AUFILETYPE
name2type(const char* path)
{
	char *suff = NULL;
	if (path == NULL)
		return AU_FILETYPE_UNKNOWN;
	if (0 == strcmp(path, "-"))
		return AU_FILETYPE_RAW;
	if ((suff = strrchr(path, '.')) == NULL)
		return AU_FILETYPE_UNKNOWN;
	return suff2type(++suff);
}

AUFILE*
au_open(const char* path, AUMODE mode, AUINFO* info)
{
	mode_t rw = 0 ;
	AUFILE *file = NULL;
	if (info == NULL)
		return NULL;
	if (path == NULL)
		return NULL;
	if (strlen(path) == 0)
		return NULL;
	if (info->filetype == AU_FILETYPE_UNKNOWN)
		info->filetype = name2type(path);
	if (info->filetype == AU_FILETYPE_UNKNOWN) {
		warnx("Filetype of '%s' cannot be determined.", path);
		return NULL;
	}
	if (info->filetype == AU_FILETYPE_RAW || mode == AU_WRITE) {
		if (info->srate == 0) {
			warnx("'%s' has no sample rate", path);
			return NULL;
		}
		if ((info->encoding & AU_ENCTYPE_MASK) == 0) {
			warnx("'%s' has no encoding type", path);
			return NULL;
		}
		if ((info->encoding & AU_ENCODING_MASK) == 0) {
			warnx("'%s' has no encoding", path);
			return NULL;
		}
		if ((info->encoding & AU_BITSIZE_MASK) == 0) {
			warnx("'%s' has no bitsize", path);
			return NULL;
		}
		if ((info->encoding & AU_ORDER_MASK) == 0
		&&  (info->encoding & AU_BITSIZE_MASK) > 8) {
			warnx("'%s' has no byteorder", path);
			return NULL;
		}
		if (info->channels == 0) {
			warnx("'%s' has no channels", path);
			return NULL;
		}
	}
	if ((file = calloc(1, sizeof(AUFILE))) == NULL)
		err(1, NULL);
	if (strcmp(path, "-") == 0) {
		if (mode == AU_READ) {
			printf("Reading stdin\n");
			file->fd = STDIN_FILENO;
		} else if (mode == AU_WRITE) {
			printf("Writing stdout\n");
			file->fd = STDOUT_FILENO;
		}
	} else {
		rw = mode == AU_READ
			? O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
		if ((file->fd = open(path, rw, 0644)) == -1) {
			warnx("'%s': %s", path, strerror(errno));
			goto err;
		}
	}
	file->mode = mode;
	file->path = strdup(path);
	switch (info->filetype) {
		case AU_FILETYPE_RAW:
			break;
		case AU_FILETYPE_WAV:
			break;
		default:
			warnx("Unknown filetype of %s", path);
			goto err;
			break;
	}
	file->info = info;
	/* FIXME: Set the header reading/writing functions */
	if (file->mode == AU_READ) {
		/* FIXME: When reading a known filetype, parse the header
		* and fill info accordingly */
	}
	/* Set the sample reading/writing functions */
	switch (info->encoding & AU_ENCTYPE_MASK) {
		case AU_ENCTYPE_PCM:
			if (pcm_init(file)) {
				warnx("Could not init file as PCM");
				goto err;
			}
			break;
		default:
			warnx("Unknown encoding type for '%s'", file->path);
			goto err;
			break;
	}
	if (file->mode == AU_WRITE) {
		/* FIXME: when writing, write the header now. */
	}
	return file;
err:
	free(file);
	return NULL;
}

void
print_encoding(uint32_t encoding)
{
	switch (encoding & AU_ENCTYPE_MASK) {
		case AU_ENCTYPE_PCM:
			printf("PCM");
			break;
		default:
			break;
	}
	switch (encoding & AU_ENCODING_MASK) {
		case AU_ENCODING_SIGNED:
			printf(", signed");
			break;
		case AU_ENCODING_UNSIGNED:
			printf(", unsigned");
			break;
		case AU_ENCODING_FLOAT:
			printf(", float");
			break;
		default:
			break;
	}
	switch (encoding & AU_BITSIZE_MASK) {
		case 8:
		case 16:
		case 32:
		default:
			printf(", %u bits", encoding & AU_BITSIZE_MASK);
			break;
	}
	switch (encoding & AU_ORDER_MASK) {
		case AU_ORDER_NONE:
			break;
		case AU_ORDER_LE:
			printf(", little-endian");
			break;
		case AU_ORDER_BE:
			printf(", big-endian");
			break;
		default:
			printf(", unknown byteorder");
			break;
	}
}

void
au_info(AUFILE *f)
{
	if (f) {
		if (f->path)
			printf("%s: ", strncmp(f->path, "-", 1) ? f->path
			: (f->fd == STDIN_FILENO ? "(stdin)" : "(stdout)"));
		if (f->info->filetype)
			printf("%s", filetypes[f->info->filetype].name);
		if (f->info->channels) {
			if (f->info->channels == 1) {
				printf(", mono");
			} else if (f->info->channels == 2) {
				printf(", stereo");
			} else {
				printf(", %u channels", f->info->channels);
			}
		}
		if (f->info->srate)
			printf(", %u Hz", f->info->srate);
		if (f->info->encoding) {
			printf(", ");
			print_encoding(f->info->encoding);
		}
		putchar('\n');
	}
}

int
au_close(AUFILE *file)
{
	if (file) {
		/*au_info(file);*/
		if (file->fd) {
			/* FIXME fix length in the header if we are writing
			* and the file is seekable. */
			return close(file->fd) == 0 ? 0 : -1;
		}
	}
	return -1;
}

ssize_t
au_read_s8(AUFILE* file, int8_t* samples, size_t len)
{
	return file->au_read_s8(file->fd, samples, len);
}

ssize_t
au_write_s8(AUFILE* file, const int8_t* samples, size_t len)
{
	/* FIXME: Properly increment samples/frames elsewhere, too.
	 * Should we take the number of _frames_ as argument?
	 * Then the r/w functions would need more than a fd */
	ssize_t n;
	n = file->au_write_s8(file->fd, samples, len);
	if (n >= 0) {
		file->info->samples += n;
		return n;
	}
	return -1;
}

ssize_t
au_read_u8(AUFILE* file, uint8_t* samples, size_t len)
{
	return file->au_read_u8(file->fd, samples, len);
}

ssize_t
au_write_u8(AUFILE* file, const uint8_t* samples, size_t len)
{
	return file->au_write_u8(file->fd, samples, len);
}

ssize_t
au_read_s16(AUFILE* file, int16_t* samples, size_t len)
{
	return file->au_read_s16(file->fd, samples, len);
}

ssize_t
au_write_s16(AUFILE* file, const int16_t* samples, size_t len)
{
	return file->au_write_s16(file->fd, samples, len);
}

ssize_t
au_read_u16(AUFILE* file, uint16_t* samples, size_t len)
{
	return file->au_read_u16(file->fd, samples, len);
}

ssize_t
au_write_u16(AUFILE* file, const uint16_t* samples, size_t len)
{
	return file->au_write_u16(file->fd, samples, len);
}

ssize_t
au_read_s32(AUFILE* file, int32_t* samples, size_t len)
{
	return file->au_read_s32(file->fd, samples, len);
}

ssize_t
au_write_s32(AUFILE* file, const int32_t* samples, size_t len)
{
	return file->au_write_s32(file->fd, samples, len);
}

ssize_t
au_read_u32(AUFILE* file, uint32_t* samples, size_t len)
{
	return file->au_read_u32(file->fd, samples, len);
}

ssize_t
au_write_u32(AUFILE* file, const uint32_t* samples, size_t len)
{
	return file->au_write_u32(file->fd, samples, len);
}

ssize_t
au_read_f32(AUFILE* file, float* samples, size_t len)
{
	return file->au_read_f32(file->fd, samples, len);
}

ssize_t
au_write_f32(AUFILE* file, const float* samples, size_t len)
{
	return file->au_write_f32(file->fd, samples, len);
}
