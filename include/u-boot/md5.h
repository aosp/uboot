/*
 * This file was transplanted with slight modifications from Linux sources
 * (fs/cifs/md5.h) into U-Boot by Bartlomiej Sieka <tur@semihalf.com>.
 */

#ifndef _MD5_H
#define _MD5_H

#include "compiler.h"

struct MD5Context {
	__u32 buf[4];
	__u32 bits[2];
	unsigned char in[64];
};

/*
 * Calculate and store in 'output' the MD5 digest of 'len' bytes at
 * 'input'. 'output' must have enough space to hold 16 bytes.
 */
void md5 (unsigned char *input, int len, unsigned char output[16]);

/*
 * Calculate and store in 'output' the MD5 digest of 'len' bytes at 'input'.
 * 'output' must have enough space to hold 16 bytes. If 'chunk' Trigger the
 * watchdog every 'chunk_sz' bytes of input processed.
 */
void md5_wd (unsigned char *input, int len, unsigned char output[16],
		unsigned int chunk_sz);

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
void MD5Init(struct MD5Context *ctx);
/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void MD5Update(struct MD5Context *ctx, unsigned char const *buf, unsigned len);
/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
#endif /* _MD5_H */
