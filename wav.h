#ifndef __AU_WAV_H_
#define __AU_WAV_H_

#include "audio.h"

struct wavhdr {
	char	fmt[4];
};

int wav_init(AUFILE *);

#endif
