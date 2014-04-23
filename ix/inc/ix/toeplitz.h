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

#pragma once

#include <ix/types.h>

#define TOEPLITZ_KEYSEED_CNT	2

extern uint32_t toeplitz_cache[TOEPLITZ_KEYSEED_CNT][256];

void toeplitz_get_key(uint8_t *_key, int _keylen);

static __inline uint32_t
toeplitz_rawhash_addrport(uint32_t _faddr, uint32_t _laddr,
			  uint16_t _fport, uint16_t _lport)
{
	uint32_t _res;

	_res =  toeplitz_cache[0][_faddr & 0xff];
	_res ^= toeplitz_cache[0][(_faddr >> 16) & 0xff];
	_res ^= toeplitz_cache[0][_laddr & 0xff];
	_res ^= toeplitz_cache[0][(_laddr >> 16) & 0xff];
	_res ^= toeplitz_cache[0][_fport & 0xff];
	_res ^= toeplitz_cache[0][_lport & 0xff];

	_res ^= toeplitz_cache[1][(_faddr >> 8) & 0xff];
	_res ^= toeplitz_cache[1][(_faddr >> 24) & 0xff];
	_res ^= toeplitz_cache[1][(_laddr >> 8) & 0xff];
	_res ^= toeplitz_cache[1][(_laddr >> 24) & 0xff];
	_res ^= toeplitz_cache[1][(_fport >> 8) & 0xff];
	_res ^= toeplitz_cache[1][(_lport >> 8) & 0xff];

	return _res;
}
