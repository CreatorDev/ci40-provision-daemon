/***************************************************************************************************
 * Copyright (c) 2016, Imagination Technologies Limited and/or its affiliated group companies
 * and/or licensors
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions
 *    and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#define PRINT_BYTES(x,len) {int aai; printf("[");for(aai = 0; aai < len; aai++) { printf("%x",x[aai]);} printf("]\n");}

#define G_FREE_AND_NULL( x ) g_free( x ); x = NULL;

/** Calculate size of array. */
#undef ARRAY_SIZE
#define ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))

/**
 * @brief Converts given hex string into array of bytes.
 * @param hexstr string to convert
 * @param dst bufer in which result will be stroed.
 * @param len length of dst buffer.
 */
void HexStringToByteArray(const char* hexstr, uint8_t * dst, size_t len);

bool GenerateRandomX(unsigned char* array, int length);

void GenerateClickerNameHash(char *buffer);

void GenerateClickerTimeHash(char *buffer);

void GenerateClickerName(char* outBuffer, int maxBufLen, char *pattern, char *hash, char *ip);

#endif /* utils_h */
