/* yubikey.h --- Prototypes for low-level YubiKey OTP functions.
 *
 * Written by Simon Josefsson <simon@josefsson.org>.
 * Copyright (c) 2006-2012 Yubico AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef YUBIKEY_H
#define YUBIKEY_H

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define YUBIKEY_BLOCK_SIZE 16
#define YUBIKEY_KEY_SIZE 16
#define YUBIKEY_UID_SIZE 6
#define YUBIKEY_OTP_SIZE (2 * YUBIKEY_BLOCK_SIZE)

  typedef struct
  {
    /* Unique (secret) ID. */
    uint8_t uid[YUBIKEY_UID_SIZE];
    /* Session counter (incremented by 1 at each startup).  High bit
       indicates whether caps-lock triggered the token. */
    uint16_t ctr;
    /* Timestamp incremented by approx 8Hz (low part). */
    uint16_t tstpl;
    /* Timestamp (high part). */
    uint8_t tstph;
    /* Number of times used within session + activation flags. */
    uint8_t use;
    /* Pseudo-random value. */
    uint16_t rnd;
    /* CRC16 value of all fields. */
    uint16_t crc;
  } yubikey_token_st;

  typedef yubikey_token_st *yubikey_token_t;

/* High-level functions. */


/* Decrypt TOKEN using KEY and store output in OUT structure.  Note
   that there is no error checking whether the output data is valid or
   not, use yubikey_check_* for that. */
  extern void yubikey_parse (const uint8_t token[YUBIKEY_BLOCK_SIZE],
			     const uint8_t key[YUBIKEY_KEY_SIZE],
			     yubikey_token_t out);

/* Generate OTP */
  extern void yubikey_generate (yubikey_token_t token,
				const uint8_t key[YUBIKEY_KEY_SIZE],
				char out[YUBIKEY_OTP_SIZE + 1]);

#define yubikey_counter(ctr) ((ctr) & 0x7FFF)
#define yubikey_capslock(ctr) ((ctr) & 0x8000)
#define yubikey_crc_ok_p(tok) \
  (yubikey_crc16 ((tok), YUBIKEY_BLOCK_SIZE) == YUBIKEY_CRC_OK_RESIDUE)

/*
 * Low-level functions; ModHex.
 */

#define YUBIKEY_MODHEX_MAP "cbdefghijklnrtuv"

/* ModHex encode input string SRC of length SRCSIZE and put the zero
   terminated output string in DST.  The size of the output string DST
   must be at least 2*SRCSIZE+1.  The output string is always
   2*SRCSIZE large plus the terminating zero.  */
  extern void yubikey_modhex_encode (char *dst,
				     const char *src, size_t srcsize);

/* ModHex decode input string SRC of length DSTSIZE/2 into output
   string DST.  The output string DST is always DSTSIZE/2 large plus
   the terminating zero.  */
  extern void yubikey_modhex_decode (char *dst,
				     const char *src, size_t dstsize);

/* Hex encode/decode data, same interface as modhex functions. */
  extern void yubikey_hex_encode (char *dst, const char *src, size_t srcsize);
  extern void yubikey_hex_decode (char *dst, const char *src, size_t dstsize);

/* Return non-zero if zero-terminated input STR is a valid (mod)hex
   string, and zero if any non-alphabetic characters are found. */
  extern int yubikey_modhex_p (const char *str);
  extern int yubikey_hex_p (const char *str);

/*
 * Low-level functions; CRC.
 */

#define YUBIKEY_CRC_OK_RESIDUE 0xf0b8
  extern uint16_t yubikey_crc16 (const uint8_t * buf, size_t buf_size);

/* Low-level functions; AES. */

/* AES-decrypt/encrypt one 16-byte block STATE using the 128-bit KEY,
   leaving the decrypted/encrypted output in the STATE buffer. */
  extern void yubikey_aes_decrypt (uint8_t * state, const uint8_t * key);
  extern void yubikey_aes_encrypt (uint8_t * state, const uint8_t * key);

#ifdef __cplusplus
}
#endif

#endif
