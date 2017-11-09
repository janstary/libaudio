#ifndef __AU_AUDIO_H_
#define __AU_AUDIO_H_

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>


typedef enum {
#define NUMTYPES 3
	AU_FILETYPE_UNKNOWN	= 0x0000,
	AU_FILETYPE_RAW		= 0x0001,
	AU_FILETYPE_WAV		= 0x0002
} AUFILETYPE;

typedef enum {
	AU_READ			= 0x0000,
	AU_WRITE		= 0x0001
} AUMODE;

/* The encoding is completely described in four bytes, specifying
 * the encoding type, the sample encoding, byteorder, and bitsize;
 * e.g. PCM, signed integers, little endian, 16 bits.
 * The first three are #defined as constants,
 * the bitsize is just a number itself. */

#define AU_ENCTYPE_MASK		0xff000000
#define AU_ENCODING_MASK	0x00ff0000
#define AU_ORDER_MASK		0x0000ff00
#define AU_BITSIZE_MASK		0x000000ff

#define AU_ENCTYPE_UNKNOWN	0x00000000
#define AU_ENCTYPE_PCM		0x01000000

#define AU_ENCODING_UNKNOWN	0x00000000
#define AU_ENCODING_SIGNED	0x00010000
#define AU_ENCODING_UNSIGNED	0x00020000
#define AU_ENCODING_FLOAT	0x00030000

#define AU_ORDER_NONE		0x00000000
#define AU_ORDER_LE		0x00000100
#define AU_ORDER_BE		0x00000200

typedef struct info {
	AUFILETYPE	filetype;
	uint16_t	srate;
	uint32_t	encoding;
	uint8_t		channels;
	uint32_t	frames;
	uint32_t	samples;
	double		seconds;
} AUINFO;

typedef struct aufile {
	int		fd;
	char*		path;
	AUMODE		mode;
	AUINFO		*info;

	ssize_t		(*au_read_s8)  (int,         int8_t*, size_t);
	ssize_t		(*au_read_u8)  (int,        uint8_t*, size_t);
	ssize_t		(*au_read_s16) (int,        int16_t*, size_t);
	ssize_t		(*au_read_u16) (int,       uint16_t*, size_t);
	ssize_t		(*au_read_s32) (int,        int32_t*, size_t);
	ssize_t		(*au_read_u32) (int,       uint32_t*, size_t);
	ssize_t		(*au_read_f32) (int,          float*, size_t);

	ssize_t		(*au_write_s8) (int, const   int8_t*, size_t);
	ssize_t		(*au_write_u8) (int, const  uint8_t*, size_t);
	ssize_t		(*au_write_s16)(int, const  int16_t*, size_t);
	ssize_t		(*au_write_u16)(int, const uint16_t*, size_t);
	ssize_t		(*au_write_s32)(int, const  int32_t*, size_t);
	ssize_t		(*au_write_u32)(int, const uint32_t*, size_t);
	ssize_t		(*au_write_f32)(int, const    float*, size_t);
} AUFILE;


/* audio.c */
AUFILETYPE suff2type	(const char*);
AUFILETYPE name2type	(const char*);
void	print_encoding	(uint32_t);

AUFILE*	au_open		(const char*, AUMODE, AUINFO*);
void	au_info		(AUFILE*);
int	au_close	(AUFILE*);

ssize_t	au_read_s8	(AUFILE*,         int8_t*, size_t);
ssize_t	au_read_u8	(AUFILE*,        uint8_t*, size_t);
ssize_t	au_read_s16	(AUFILE*,        int16_t*, size_t);
ssize_t	au_read_u16	(AUFILE*,       uint16_t*, size_t);
ssize_t	au_read_s32	(AUFILE*,        int32_t*, size_t);
ssize_t	au_read_u32	(AUFILE*,       uint32_t*, size_t);
ssize_t	au_read_f32	(AUFILE*,          float*, size_t);

ssize_t	au_write_s8	(AUFILE*, const   int8_t*, size_t);
ssize_t	au_write_u8	(AUFILE*, const  uint8_t*, size_t);
ssize_t	au_write_s16	(AUFILE*, const  int16_t*, size_t);
ssize_t	au_write_u16	(AUFILE*, const uint16_t*, size_t);
ssize_t	au_write_s32	(AUFILE*, const  int32_t*, size_t);
ssize_t	au_write_u32	(AUFILE*, const uint32_t*, size_t);
ssize_t	au_write_f32	(AUFILE*, const    float*, size_t);

#endif
