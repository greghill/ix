/*
 * Copyright (c) 2009 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Toeplitz hash function
 *
 * This function is used to support Receive Side Scaling:
 * http://www.microsoft.com/whdc/device/network/ndis_rss.mspx
 *
 * Two things are changed from the above paper:
 * o  Instead of creating random 40 bytes key string, we replicate
 *    2 user defined bytes to form the 40 bytes key string.  So the
 *    hash result of TCP segment is commutative.  '2' is chosen,
 *    since the hash is calculated upon the binary string formed by
 *    concatenating faddr,laddr,fport,lport; the smallest unit is
 *    the size of the fport/lport, which is 2 bytes.
 * o  Precalculated hash result cache is used to reduce the heavy
 *    computation burden
 *
 * Thank Simon 'corecode' Schubert <corecode@fs.ei.tum.de> very much
 * for various constructive suggestions.  Without him, this will not
 * be possible.
 */

#include <strings.h>

#include <ix/toeplitz.h>

#define NBBY			8
#define TOEPLITZ_KEYSEED0	0x6d
#define TOEPLITZ_KEYSEED1	0x5a
#define TOEPLITZ_INIT_KEYLEN	(TOEPLITZ_KEYSEED_CNT + sizeof(uint32_t))

static uint32_t toeplitz_keyseeds[TOEPLITZ_KEYSEED_CNT] =
	{ TOEPLITZ_KEYSEED0, TOEPLITZ_KEYSEED1 };

uint32_t toeplitz_cache[TOEPLITZ_KEYSEED_CNT][256];

static void
toeplitz_cache_create(uint32_t cache[][256], int cache_len,
		      const uint8_t key_str[], int key_strlen)
{
	int i;

	for (i = 0; i < cache_len; ++i) {
		uint32_t key[NBBY];
		int j, b, shift, val;

		bzero(key, sizeof(key));

		/*
		 * Calculate 32bit keys for one byte; one key for each bit.
		 */
		for (b = 0; b < NBBY; ++b) {
			for (j = 0; j < 32; ++j) {
				uint8_t k;
				int bit;

				bit = (i * NBBY) + b + j;

				k = key_str[bit / NBBY];
				shift = NBBY - (bit % NBBY) - 1;
				if (k & (1 << shift))
					key[b] |= 1 << (31 - j);
			}
		}

		/*
		 * Cache the results of all possible bit combination of
		 * one byte.
		 */
		for (val = 0; val < 256; ++val) {
			uint32_t res = 0;

			for (b = 0; b < NBBY; ++b) {
				shift = NBBY - b - 1;
				if (val & (1 << shift))
					res ^= key[b];
			}
			cache[i][val] = res;
		}
	}
}

void
toeplitz_init(void)
{
	uint8_t key[TOEPLITZ_INIT_KEYLEN];
	int i;

	for (i = 0; i < TOEPLITZ_KEYSEED_CNT; ++i)
		toeplitz_keyseeds[i] &= 0xff;

	toeplitz_get_key(key, TOEPLITZ_INIT_KEYLEN);

	toeplitz_cache_create(toeplitz_cache, TOEPLITZ_KEYSEED_CNT,
			      key, TOEPLITZ_INIT_KEYLEN);
}

void
toeplitz_get_key(uint8_t *key, int keylen)
{
	int i;

	/* Replicate key seeds to form key */
	for (i = 0; i < keylen; ++i)
		key[i] = toeplitz_keyseeds[i % TOEPLITZ_KEYSEED_CNT];
}
