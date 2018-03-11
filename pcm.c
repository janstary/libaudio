#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

#include "audio.h"
#include "pcm.h"

/* These are the linear PCM reading and writing functions.
 * The names follow a "pcm_{read,write}_src_as_dst" pattern:
 * samples in the src format are read/written in the dst format,
 * e.g. pcm_read_s16le_as_u8() reads signed LE shorts as unsigned chars,
 * and pcm_write_s8_as_s32be() writes signed chars as signed BE ints.
 * Note that the byte order of the machine running this is irrelevant;
 * samples are always stored in memory in the native byte order.
 * The r/w functions return the number of samples read/written, or -1. */

#define BUFSIZE  (32 * 1024)
#define MIN(x,y) ((x) < (y) ? (x) : (y))


/* Multibyte integer reading and writing routines.
 * The extra variable makes it possible to use e.g. W16LE(p, *samples++)
 * without having 'samples' incremented more than once. */

#define R16LE(p) ( (p[0] << 0) | (p[1] << 8) )
#define R16BE(p) ( (p[1] << 0) | (p[0] << 8) )
#define R32LE(p) ( (p[0] << 0) | (p[1] << 8) | (p[2] << 16) | (p[3] << 24) )
#define R32BE(p) ( (p[3] << 0) | (p[2] << 8) | (p[1] << 16) | (p[0] << 24) )

#define W16LE(p,s) {		\
	uint16_t _s = s;	\
	p[0] = ((_s) >> 0);	\
	p[1] = ((_s) >> 8);	\
}

#define W16BE(p,s) {		\
	uint16_t _s = s;	\
	p[0] = ((_s) >> 8);	\
	p[1] = ((_s) >> 0);	\
}

#define W32LE(p,s) {		\
	uint32_t _s = s;	\
	p[0] = ((_s) >>  0);	\
	p[1] = ((_s) >>  8);	\
	p[2] = ((_s) >> 16);	\
	p[3] = ((_s) >> 24);	\
}

#define W32BE(p,s) {		\
	uint32_t _s = s;	\
	p[0] = ((_s) >> 24);	\
	p[1] = ((_s) >> 16);	\
	p[2] = ((_s) >>  8);	\
	p[3] = ((_s) >>  0);	\
}

/* Float reading and writing routines. */

typedef union {
	uint32_t u;
	float f;
} ufloat;

static inline float
RFLE(unsigned char* p)
{
	ufloat uf;
	uf.u = (uint32_t)R32LE(p);
	return uf.f;
}

static inline float
RFBE(unsigned char* p)
{
	ufloat uf;
	uf.u = (uint32_t)R32BE(p);
	return uf.f;
}

#define WFLE(p,s) { ufloat uf; uf.f = s; W32LE(p, uf.u); }
#define WFBE(p,s) { ufloat uf; uf.f = s; W32BE(p, uf.u); }


/* int8_t */

static ssize_t
pcm_read_s8_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t r = 0;
	if ((r = read(fd, samples, len)) == -1)
		err(1, NULL);
	return r;
}

static ssize_t
pcm_write_s8_as_s8(int fd, const int8_t *samples, size_t len)
{
	ssize_t w = 0;
	if ((w = write(fd, samples, len)) == -1)
		err(1, NULL);
	return w;
}

static ssize_t
pcm_read_s8_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r;
	if ((r = read(fd, samples, len)) == -1)
		err(1, NULL);
	for (i = 0; i < r ; i++)
		samples[i] += 0x80;
	return r;
}

static ssize_t
pcm_write_s8_as_u8(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen; i++)
			buf[i] = *samples++ + 0x80;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_s8_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = buf[i] << 8;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_s16le(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_s16be(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s8_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = (buf[i] + 0x80) << 8;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_u16le(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, (*samples++ + 0x80) << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_u16be(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, (*samples++ + 0x80) << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s8_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = buf[i] << 24;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_s32le(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_s32be(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s8_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = (buf[i] + 0x80) << 24;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_u32le(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, (*samples++ + 0x80) << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_u32be(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, (*samples++ + 0x80) << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s8_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = buf[i] > 0
				? ( 1.0 * buf[i]) / INT8_MAX
				: (-1.0 * buf[i]) / INT8_MIN;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_f32le(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFLE(p, *samples > 0
				? (*samples *  1.0) / INT8_MAX
				: (*samples * -1.0) / INT8_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

static ssize_t
pcm_write_s8_as_f32be(int fd, const int8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFLE(p, *samples > 0
				? (*samples *  1.0) / INT8_MAX
				: (*samples * -1.0) / INT8_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

/* uint8_t */

static ssize_t
pcm_read_u8_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, n;
	if ((n = read(fd, samples, len)) == -1)
		err(1, NULL);
	for (i = 0; i < n ; i++)
		samples[i] -= 0x80;
	return n;
}

static ssize_t
pcm_write_u8_as_s8(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen; i++)
			buf[i] = *samples++ - 0x80;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_u8_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t n;
	if ((n = read(fd, samples, len)) == -1)
		err(1, NULL);
	return n;
}

static ssize_t
pcm_write_u8_as_u8(int fd, const uint8_t *samples, size_t len)
{
	ssize_t n;
	if ((n = write(fd, samples, len)) == -1)
		err(1, NULL);
	return n;
}

static ssize_t
pcm_read_u8_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = (buf[i] - 0x80) << 8;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_s16le(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, (*samples++ - 0x80) << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_s16be(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, (*samples++ - 0x80) << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u8_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = buf[i] << 8;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_u16le(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_u16be(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ << 8);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u8_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = (buf[i] - 0x80) << 24;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_s32le(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, (*samples++ - 0x80) << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_s32be(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, (*samples++ - 0x80) << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u8_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = buf[i] << 24;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_u32le(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_u32be(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ << 24);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u8_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen)) == -1)
			err(1, NULL);
		for (i = 0; i < r ; i++)
			*samples++ = -1.0 + (2.0 * buf[i]) / UINT8_MAX;
		len -= r;
		tot += r;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_f32le(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFLE(p, -1.0 + (*samples++ * 2.0) / UINT8_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

static ssize_t
pcm_write_u8_as_f32be(int fd, const uint8_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFBE(p, -1.0 + (*samples++ * 2.0) / UINT8_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

/* int16_t */

static ssize_t
pcm_read_s16le_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16LE(p)) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16BE(p)) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_s8(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = *samples++ >> 8;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((int16_t)R16LE(p)) >> 8) + 0x80;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((int16_t)R16BE(p)) >> 8) + 0x80;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_u8(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = (*samples++ >> 8) + 0x80;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (int16_t)R16LE(p);
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (int16_t)R16BE(p);
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_s16le(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_s16be(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16LE(p)) + 0x8000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16BE(p)) + 0x8000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_u16le(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ + 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_u16be(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ + 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16LE(p)) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((int16_t)R16BE(p)) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_s32le(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_s32be(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((int16_t)R16LE(p)) << 16) + 0x80000000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((int16_t)R16BE(p)) << 16) + 0x80000000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_u32le(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, (*samples++ << 16) + 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_u32be(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, (*samples++ << 16) + 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s16le_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2, samples++) {
			*samples = (int16_t)R16LE(p);
			*samples /= *samples > 0 ? INT16_MAX : -INT16_MIN;
		}
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_s16be_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2, samples++) {
			*samples = (int16_t)R16BE(p);
			*samples /= *samples > 0 ? INT16_MAX : -INT16_MIN;
		}
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_f32le(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFLE(p, *samples > 0
				? (*samples *  1.0) / INT16_MAX
				: (*samples * -1.0) / INT16_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

static ssize_t
pcm_write_s16_as_f32be(int fd, const int16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFBE(p, *samples > 0
				? (*samples *  1.0) / INT16_MAX
				: (*samples * -1.0) / INT16_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

/* uint16_t */

static ssize_t
pcm_read_u16le_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((uint16_t)R16LE(p)) - 0x8000) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((uint16_t)R16BE(p)) - 0x8000) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_s8(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = (*samples++ - 0x8000) >> 8;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16LE(p)) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16BE(p)) >> 8;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_u8(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = *samples++ >> 8;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16LE(p)) - 0x8000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16BE(p)) - 0x8000;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_s16le(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ - 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_s16be(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ - 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (uint16_t)R16LE(p);
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (uint16_t)R16BE(p);
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_u16le(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_u16be(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((uint16_t)R16LE(p)) - 0x8000) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = (((uint16_t)R16BE(p)) - 0x8000) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_s32le(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, (*samples++ - 0x8000) << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_s32be(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, (*samples++ - 0x8000) << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16LE(p)) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = ((uint16_t)R16BE(p)) << 16;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_u32le(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_u32be(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ << 16);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u16le_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = -1.0+(2.0*((uint16_t)R16LE(p)))/UINT16_MAX;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_read_u16be_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 2, p += 2)
			*samples++ = -1.0+(2.0*((uint16_t)R16BE(p)))/UINT16_MAX;
		len -= r/2;
		tot += r/2;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_f32le(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFLE(p, -1.0 + (*samples++ * 2.0) / UINT16_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u16_as_f32be(int fd, const uint16_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFBE(p, -1.0 + (*samples++ * 2.0) / UINT16_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

/* int32_t */

static ssize_t
pcm_read_s32le_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32LE(p)) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32BE(p)) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_s8(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = *samples++ >> 24;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((int32_t)R32LE(p)) >> 24) + 0x80;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((int32_t)R32BE(p)) >> 24) + 0x80;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_u8(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	uint8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = (*samples++ >> 24) + 0x80;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32LE(p)) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32BE(p)) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_s16le(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ >> 16);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_s16be(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ >> 16);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((int32_t)R32LE(p)) >> 16) + 0x8000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((int32_t)R32BE(p)) >> 16) + 0x8000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_u16le(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, (*samples++ >> 16) + 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_u16be(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, (*samples++ >> 16) + 0x8000);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (int32_t)R32LE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (int32_t)R32BE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_s32le(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_s32be(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32LE(p)) + 0x80000000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((int32_t)R32BE(p)) + 0x80000000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_u32le(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ + 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_u32be(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ + 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32le_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4, samples++) {
			*samples = (int32_t)R32LE(p);
			*samples /= *samples > 0 ? INT32_MAX : -1.0 * INT32_MIN;
		}
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_s32be_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4, samples++) {
			*samples = (int32_t)R32BE(p);
			*samples /= *samples>0 ? INT32_MAX : -1.0 * INT32_MIN;
		}
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_f32le(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFLE(p, *samples > 0
				? (*samples *  1.0) / INT32_MAX
				: (*samples * -1.0) / INT32_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

static ssize_t
pcm_write_s32_as_f32be(int fd, const int32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			WFBE(p, *samples > 0
				? (*samples *  1.0) / INT32_MAX
				: (*samples * -1.0) / INT32_MIN);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w / 4;
	}
	return tot;
}

/* uint32_t */

static ssize_t
pcm_read_u32le_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((uint32_t)R32LE(p)) - 0x80000000 ) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_s8(int fd, int8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((uint32_t)R32BE(p)) - 0x80000000 ) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_s8(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = (*samples++ - 0x80000000) >> 24;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32LE(p)) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32BE(p)) >> 24;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_u8(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = *samples++ >> 24;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((uint32_t)R32LE(p)) - 0x80000000) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_s16(int fd, int16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (((uint32_t)R32BE(p)) - 0x80000000) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_s16le(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, (*samples++ - 0x80000000) >> 16);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_s16be(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, (*samples++ - 0x80000000) >> 16);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32LE(p)) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32BE(p)) >> 16;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_u16le(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, *samples++ >> 16);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_u16be(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, *samples++ >> 16);
			/* FIXME: je vsude spravne [RW][16|32][BL]? */
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32LE(p)) - 0x80000000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_s32(int fd, int32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((uint32_t)R32BE(p)) - 0x80000000;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_s32le(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++ - 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_s32be(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++ - 0x80000000);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (uint32_t)R32LE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (uint32_t)R32BE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_u32le(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_u32be(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32le_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = -1.0+(2.0*((uint32_t)R32LE(p)))/UINT32_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_u32be_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = -1.0+(2.0*((uint32_t)R32BE(p)))/UINT32_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_f32le(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFLE(p, -1.0 + (*samples++ * 2.0) / UINT32_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_u32_as_f32be(int fd, const uint32_t *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFBE(p, -1.0 + (*samples++ * 2.0) / UINT32_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

/* float */

static ssize_t
pcm_read_f32le_as_s8(int fd, int8_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((f=RFLE(p))>0) ? f*INT8_MAX : -f*INT8_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_s8(int fd, int8_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = (f=RFBE(p) > 0) ? f*INT8_MAX : -f*INT8_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_s8(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++, samples++)
			buf[i] = *samples > 0
				? *samples * INT8_MAX
				: *samples * INT8_MIN * -1.0;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFLE(p)) / 2.0) * UINT8_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_u8(int fd, uint8_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFBE(p)) / 2.0) * UINT8_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_u8(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	int8_t buf[BUFSIZE];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0; i < buflen ; i++)
			buf[i] = ((1.0 + *samples++) / 2.0) * UINT8_MAX;
		if ((w = write(fd, buf, buflen)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_s16(int fd, int16_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ =((f=RFLE(p))>0) ? f*INT16_MAX: -f*INT16_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_s16(int fd, int16_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ =((f=RFBE(p))>0) ? f*INT16_MAX: -f*INT16_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_s16le(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2, samples++)
			W16LE(p, *samples > 0
				? *samples * INT16_MAX
				: *samples * INT16_MIN * -1.0);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_s16be(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2, samples++)
			W16BE(p, *samples > 0
				? *samples * INT16_MAX
				: *samples * INT16_MIN * -1.0);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFLE(p)) / 2.0) * UINT16_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_u16(int fd, uint16_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFBE(p)) / 2.0) * UINT16_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_u16le(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16LE(p, ((1.0 + *samples++) / 2.0) * UINT16_MAX);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_u16be(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 2];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 2)
			W16BE(p, ((1.0 + *samples++) / 2.0) * UINT16_MAX);
		if ((w = write(fd, buf, buflen * 2)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/2;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_s32(int fd, int32_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ =((f=RFLE(p))>0) ? f*INT32_MAX: -f*INT32_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_s32(int fd, int32_t *samples, size_t len)
{
	float f = 0;
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ =((f=RFBE(p))>0) ? f*INT32_MAX: -f*INT32_MIN;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_s32le(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			W32LE(p, *samples > 0
				? *samples * INT32_MAX
				: *samples * INT32_MIN * -1.0);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_s32be(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4, samples++)
			W32BE(p, *samples > 0
				? *samples * INT32_MAX
				: *samples * INT32_MIN * -1.0);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFLE(p)) / 2.0) * UINT32_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_u32(int fd, uint32_t *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = ((1.0 + RFBE(p)) / 2.0) * UINT32_MAX;
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_u32le(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32LE(p, ((1.0 + *samples++) / 2.0) * UINT32_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_u32be(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			W32BE(p, ((1.0 + *samples++) / 2.0) * UINT32_MAX);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32le_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = RFLE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_read_f32be_as_f32(int fd, float *samples, size_t len)
{
	ssize_t i, r, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		if ((r = read(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		for (i = 0, p = buf; i < r ; i += 4, p += 4)
			*samples++ = RFBE(p);
		len -= r/4;
		tot += r/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_f32le(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFLE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}

static ssize_t
pcm_write_f32_as_f32be(int fd, const float *samples, size_t len)
{
	ssize_t i, w, buflen, tot = 0;
	unsigned char *p, buf[BUFSIZE * 4];
	while (len) {
		buflen = MIN(len, BUFSIZE);
		for (i = 0, p = buf; i < buflen; i += 1, p += 4)
			WFBE(p, *samples++);
		if ((w = write(fd, buf, buflen * 4)) == -1)
			err(1, NULL);
		len -= buflen;
		tot += w/4;
	}
	return tot;
}


int
pcm_init(AUFILE *file)
{
	if (file == NULL || file->info == NULL)
		return -1;
	if ((file->info->encoding & AU_ENCTYPE_MASK) != AU_ENCTYPE_PCM) {
		warnx("Will not intitialize non PCM file as PCM");
		return -1;
	}

	if (file->mode == AU_READ) {
		switch (file->info->encoding
		& (AU_ENCODING_MASK | AU_ORDER_MASK | AU_BITSIZE_MASK)) {
		case AU_ENCODING_SIGNED | AU_ORDER_NONE | 8:
			file->au_read_s8  = pcm_read_s8_as_s8;
			file->au_read_u8  = pcm_read_s8_as_u8;
			file->au_read_s16 = pcm_read_s8_as_s16;
			file->au_read_u16 = pcm_read_s8_as_u16;
			file->au_read_s32 = pcm_read_s8_as_s32;
			file->au_read_u32 = pcm_read_s8_as_u32;
			file->au_read_f32 = pcm_read_s8_as_f32;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_NONE | 8:
			file->au_read_s8  = pcm_read_u8_as_s8;
			file->au_read_u8  = pcm_read_u8_as_u8;
			file->au_read_s16 = pcm_read_u8_as_s16;
			file->au_read_u16 = pcm_read_u8_as_u16;
			file->au_read_s32 = pcm_read_u8_as_s32;
			file->au_read_u32 = pcm_read_u8_as_u32;
			file->au_read_f32 = pcm_read_u8_as_f32;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_LE | 16:
			file->au_read_s8  = pcm_read_s16le_as_s8;
			file->au_read_u8  = pcm_read_s16le_as_u8;
			file->au_read_s16 = pcm_read_s16le_as_s16;
			file->au_read_u16 = pcm_read_s16le_as_u16;
			file->au_read_s32 = pcm_read_s16le_as_s32;
			file->au_read_u32 = pcm_read_s16le_as_u32;
			file->au_read_f32 = pcm_read_s16le_as_f32;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_BE | 16:
			file->au_read_s8  = pcm_read_s16be_as_s8;
			file->au_read_u8  = pcm_read_s16be_as_u8;
			file->au_read_s16 = pcm_read_s16be_as_s16;
			file->au_read_u16 = pcm_read_s16be_as_u16;
			file->au_read_s32 = pcm_read_s16be_as_s32;
			file->au_read_u32 = pcm_read_s16be_as_u32;
			file->au_read_f32 = pcm_read_s16be_as_f32;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_LE | 16:
			file->au_read_s8  = pcm_read_u16le_as_s8;
			file->au_read_u8  = pcm_read_u16le_as_u8;
			file->au_read_s16 = pcm_read_u16le_as_s16;
			file->au_read_u16 = pcm_read_u16le_as_u16;
			file->au_read_s32 = pcm_read_u16le_as_s32;
			file->au_read_u32 = pcm_read_u16le_as_u32;
			file->au_read_f32 = pcm_read_u16le_as_f32;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_BE | 16:
			file->au_read_s8  = pcm_read_u16be_as_s8;
			file->au_read_u8  = pcm_read_u16be_as_u8;
			file->au_read_s16 = pcm_read_u16be_as_s16;
			file->au_read_u16 = pcm_read_u16be_as_u16;
			file->au_read_s32 = pcm_read_u16be_as_s32;
			file->au_read_u32 = pcm_read_u16be_as_u32;
			file->au_read_f32 = pcm_read_u16be_as_f32;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_LE | 32:
			file->au_read_s8  = pcm_read_s32le_as_s8;
			file->au_read_u8  = pcm_read_s32le_as_u8;
			file->au_read_s16 = pcm_read_s32le_as_s16;
			file->au_read_u16 = pcm_read_s32le_as_u16;
			file->au_read_s32 = pcm_read_s32le_as_s32;
			file->au_read_u32 = pcm_read_s32le_as_u32;
			file->au_read_f32 = pcm_read_s32le_as_f32;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_BE | 32:
			file->au_read_s8  = pcm_read_s32be_as_s8;
			file->au_read_u8  = pcm_read_s32be_as_u8;
			file->au_read_s16 = pcm_read_s32be_as_s16;
			file->au_read_u16 = pcm_read_s32be_as_u16;
			file->au_read_s32 = pcm_read_s32be_as_s32;
			file->au_read_u32 = pcm_read_s32be_as_u32;
			file->au_read_f32 = pcm_read_s32be_as_f32;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_LE | 32:
			file->au_read_s8  = pcm_read_u32le_as_s8;
			file->au_read_u8  = pcm_read_u32le_as_u8;
			file->au_read_s16 = pcm_read_u32le_as_s16;
			file->au_read_u16 = pcm_read_u32le_as_u16;
			file->au_read_s32 = pcm_read_u32le_as_s32;
			file->au_read_u32 = pcm_read_u32le_as_u32;
			file->au_read_f32 = pcm_read_u32le_as_f32;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_BE | 32:
			file->au_read_s8  = pcm_read_u32be_as_s8;
			file->au_read_u8  = pcm_read_u32be_as_u8;
			file->au_read_s16 = pcm_read_u32be_as_s16;
			file->au_read_u16 = pcm_read_u32be_as_u16;
			file->au_read_s32 = pcm_read_u32be_as_s32;
			file->au_read_u32 = pcm_read_u32be_as_u32;
			file->au_read_f32 = pcm_read_u32be_as_f32;
			break;
		case AU_ENCODING_FLOAT | AU_ORDER_LE | 32:
			file->au_read_s8  = pcm_read_f32le_as_s8;
			file->au_read_u8  = pcm_read_f32le_as_u8;
			file->au_read_s16 = pcm_read_f32le_as_s16;
			file->au_read_u16 = pcm_read_f32le_as_u16;
			file->au_read_s32 = pcm_read_f32le_as_s32;
			file->au_read_u32 = pcm_read_f32le_as_u32;
			file->au_read_f32 = pcm_read_f32le_as_f32;
			break;
		case AU_ENCODING_FLOAT | AU_ORDER_BE | 32:
			file->au_read_s8  = pcm_read_f32be_as_s8;
			file->au_read_u8  = pcm_read_f32be_as_u8;
			file->au_read_s16 = pcm_read_f32be_as_s16;
			file->au_read_u16 = pcm_read_f32be_as_u16;
			file->au_read_s32 = pcm_read_f32be_as_s32;
			file->au_read_u32 = pcm_read_f32be_as_u32;
			file->au_read_f32 = pcm_read_f32be_as_f32;
			break;
		default:
			warnx("Don't know how to read this PCM:");
			print_encoding(file->info->encoding);
			return -1;
			break;
		}
	}

	if (file->mode == AU_WRITE) {
		switch (file->info->encoding
		& (AU_ENCODING_MASK | AU_ORDER_MASK | AU_BITSIZE_MASK)) {
		case AU_ENCODING_SIGNED | AU_ORDER_NONE | 8:
			file->au_write_s8  = pcm_write_s8_as_s8;
			file->au_write_u8  = pcm_write_u8_as_s8;
			file->au_write_s16 = pcm_write_s16_as_s8;
			file->au_write_u16 = pcm_write_u16_as_s8;
			file->au_write_s32 = pcm_write_s32_as_s8;
			file->au_write_u32 = pcm_write_u32_as_s8;
			file->au_write_f32 = pcm_write_f32_as_s8;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_NONE | 8:
			file->au_write_s8  = pcm_write_s8_as_u8;
			file->au_write_u8  = pcm_write_u8_as_u8;
			file->au_write_s16 = pcm_write_s16_as_u8;
			file->au_write_u16 = pcm_write_u16_as_u8;
			file->au_write_s32 = pcm_write_s32_as_u8;
			file->au_write_u32 = pcm_write_u32_as_u8;
			file->au_write_f32 = pcm_write_f32_as_u8;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_LE | 16:
			file->au_write_s8  = pcm_write_s8_as_s16le;
			file->au_write_u8  = pcm_write_u8_as_s16le;
			file->au_write_s16 = pcm_write_s16_as_s16le;
			file->au_write_u16 = pcm_write_u16_as_s16le;
			file->au_write_s32 = pcm_write_s32_as_s16le;
			file->au_write_u32 = pcm_write_u32_as_s16le;
			file->au_write_f32 = pcm_write_f32_as_s16le;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_BE | 16:
			file->au_write_s8  = pcm_write_s8_as_s16be;
			file->au_write_u8  = pcm_write_u8_as_s16be;
			file->au_write_s16 = pcm_write_s16_as_s16be;
			file->au_write_u16 = pcm_write_u16_as_s16be;
			file->au_write_s32 = pcm_write_s32_as_s16be;
			file->au_write_u32 = pcm_write_u32_as_s16be;
			file->au_write_f32 = pcm_write_f32_as_s16be;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_LE | 16:
			file->au_write_s8  = pcm_write_s8_as_u16le;
			file->au_write_u8  = pcm_write_u8_as_u16le;
			file->au_write_s16 = pcm_write_s16_as_u16le;
			file->au_write_u16 = pcm_write_u16_as_u16le;
			file->au_write_s32 = pcm_write_s32_as_u16le;
			file->au_write_u32 = pcm_write_u32_as_u16le;
			file->au_write_f32 = pcm_write_f32_as_u16le;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_BE | 16:
			file->au_write_s8  = pcm_write_s8_as_u16be;
			file->au_write_u8  = pcm_write_u8_as_u16be;
			file->au_write_s16 = pcm_write_s16_as_u16be;
			file->au_write_u16 = pcm_write_u16_as_u16be;
			file->au_write_s32 = pcm_write_s32_as_u16be;
			file->au_write_u32 = pcm_write_u32_as_u16be;
			file->au_write_f32 = pcm_write_f32_as_u16be;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_LE | 32:
			file->au_write_s8  = pcm_write_s8_as_s32le;
			file->au_write_u8  = pcm_write_u8_as_s32le;
			file->au_write_s16 = pcm_write_s16_as_s32le;
			file->au_write_u16 = pcm_write_u16_as_s32le;
			file->au_write_s32 = pcm_write_s32_as_s32le;
			file->au_write_u32 = pcm_write_u32_as_s32le;
			file->au_write_f32 = pcm_write_f32_as_s32le;
			break;
		case AU_ENCODING_SIGNED | AU_ORDER_BE | 32:
			file->au_write_s8  = pcm_write_s8_as_s32be;
			file->au_write_u8  = pcm_write_u8_as_s32be;
			file->au_write_s16 = pcm_write_s16_as_s32be;
			file->au_write_u16 = pcm_write_u16_as_s32be;
			file->au_write_s32 = pcm_write_s32_as_s32be;
			file->au_write_u32 = pcm_write_u32_as_s32be;
			file->au_write_f32 = pcm_write_f32_as_s32be;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_LE | 32:
			file->au_write_s8  = pcm_write_s8_as_u32le;
			file->au_write_u8  = pcm_write_u8_as_u32le;
			file->au_write_s16 = pcm_write_s16_as_u32le;
			file->au_write_u16 = pcm_write_u16_as_u32le;
			file->au_write_s32 = pcm_write_s32_as_u32le;
			file->au_write_u32 = pcm_write_u32_as_u32le;
			file->au_write_f32 = pcm_write_f32_as_u32le;
			break;
		case AU_ENCODING_UNSIGNED | AU_ORDER_BE | 32:
			file->au_write_s8  = pcm_write_s8_as_u32be;
			file->au_write_u8  = pcm_write_u8_as_u32be;
			file->au_write_s16 = pcm_write_s16_as_u32be;
			file->au_write_u16 = pcm_write_u16_as_u32be;
			file->au_write_s32 = pcm_write_s32_as_u32be;
			file->au_write_u32 = pcm_write_u32_as_u32be;
			file->au_write_f32 = pcm_write_f32_as_u32be;
			break;
		case AU_ENCODING_FLOAT | AU_ORDER_LE | 32:
			file->au_write_s8  = pcm_write_s8_as_f32le;
			file->au_write_u8  = pcm_write_u8_as_f32le;
			file->au_write_s16 = pcm_write_s16_as_f32le;
			file->au_write_u16 = pcm_write_u16_as_f32le;
			file->au_write_s32 = pcm_write_s32_as_f32le;
			file->au_write_u32 = pcm_write_u32_as_f32le;
			file->au_write_f32 = pcm_write_f32_as_f32le;
			break;
		case AU_ENCODING_FLOAT | AU_ORDER_BE | 32:
			file->au_write_s8  = pcm_write_s8_as_f32be;
			file->au_write_u8  = pcm_write_u8_as_f32be;
			file->au_write_s16 = pcm_write_s16_as_f32be;
			file->au_write_u16 = pcm_write_u16_as_f32be;
			file->au_write_s32 = pcm_write_s32_as_f32be;
			file->au_write_u32 = pcm_write_u32_as_f32be;
			file->au_write_f32 = pcm_write_f32_as_f32be;
			break;
		default:
			warnx("Don't know how to write this PCM:");
			print_encoding(file->info->encoding);
			/* FIXME: print_encoding() should go to stderr here*/
			return -1;
			break;
		}
	}

	return 0;
}
