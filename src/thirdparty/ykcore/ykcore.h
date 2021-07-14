/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2015 Yubico AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
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
 */

#ifndef	__YKCORE_H_INCLUDED__
#define	__YKCORE_H_INCLUDED__

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/*************************************************************************
 **
 ** N O T E :  For all functions that return a value, 0 and NULL indicates
 ** an error, other values indicate success.
 **
 ************************************************************************/

/*************************************************************************
 *
 * Structures used.  They are further defined in ykdef.h
 *
 ****/

typedef struct yk_key_st YK_KEY;	/* Really a USB device handle. */
typedef struct yk_status_st YK_STATUS;	/* Status structure,
					   filled by yk_get_status(). */

typedef struct yk_ticket_st YK_TICKET;	/* Ticket structure... */
typedef struct yk_config_st YK_CONFIG;	/* Configuration structure.
					   Other libraries provide access. */
typedef struct yk_nav_st YK_NAV;	/* Navigation structure.
					   Other libraries provide access. */
typedef struct yk_frame_st YK_FRAME;	/* Data frame for write operation */
typedef struct ndef_st YK_NDEF;
typedef struct yk_device_config_st YK_DEVICE_CONFIG;

/*************************************************************************
 *
 * Library initialisation functions.
 *
 ****/
extern int yk_init(void);
extern int yk_release(void);

/*************************************************************************
 *
 * Functions to get and release the key itself.
 *
 ****/
/* opens first key available. For backwards compatability */
extern YK_KEY *yk_open_first_key(void);
extern YK_KEY *yk_open_key(int);	/* opens nth key available */
extern YK_KEY *yk_open_key_vid_pid(const int*, size_t, const int*, size_t, int);
extern int yk_close_key(YK_KEY *k);		/* closes a previously opened key */

/*************************************************************************
 *
 * Functions to get data from the key.
 *
 ****/
/* fetches key status into the structure given by `status' */
extern int yk_get_status(YK_KEY *k, YK_STATUS *status /*, int forceUpdate */);
/* checks that the firmware revision of the key is supported */
extern int yk_check_firmware_version(YK_KEY *k);
extern int yk_check_firmware_version2(YK_STATUS *status);
/* Read the factory set serial number from a YubiKey 2.0 or higher. */
extern int yk_get_serial(YK_KEY *yk, uint8_t slot, unsigned int flags, unsigned int *serial);
/* Wait for the key to either set or clear bits in it's status byte */
extern int yk_wait_for_key_status(YK_KEY *yk, uint8_t slot, unsigned int flags,
				  unsigned int max_time_ms,
				  bool logic_and, unsigned char mask,
				  unsigned char *last_data);
/* Read the response to a command from the YubiKey */
extern int yk_read_response_from_key(YK_KEY *yk, uint8_t slot, unsigned int flags,
				     void *buf, unsigned int bufsize, unsigned int expect_bytes,
				     unsigned int *bytes_read);

/*************************************************************************
 *
 * Functions to write data to the key.
 *
 ****/

/* writes the given configuration to the key.  If the configuration is NULL,
   zap the key configuration.
   acc_code has to be provided of the key has a protecting access code. */
extern int yk_write_command(YK_KEY *k, YK_CONFIG *cfg, uint8_t command,
			   unsigned char *acc_code);
/* wrapper function of yk_write_command */
extern int yk_write_config(YK_KEY *k, YK_CONFIG *cfg, int confnum,
			   unsigned char *acc_code);
/* writes the given ndef to the key as SLOT_NDEF */
extern int yk_write_ndef(YK_KEY *yk, YK_NDEF *ndef);
/* writes the given ndef to the key. */
extern int yk_write_ndef2(YK_KEY *yk, YK_NDEF *ndef, int confnum);
/* writes a device config block to the key. */
extern int yk_write_device_config(YK_KEY *yk, YK_DEVICE_CONFIG *device_config);
/* writes a scanmap to the key. */
extern int yk_write_scan_map(YK_KEY *yk, unsigned char *scan_map);
/* Write something to the YubiKey (a command that is). */
extern int yk_write_to_key(YK_KEY *yk, uint8_t slot, const void *buf, int bufcount);
/* Do a challenge-response round with the key. */
extern int yk_challenge_response(YK_KEY *yk, uint8_t yk_cmd, int may_block,
				 unsigned int challenge_len, const unsigned char *challenge,
				 unsigned int response_len, unsigned char *response);

extern int yk_force_key_update(YK_KEY *yk);
/* Get the VID and PID of an opened device. */
extern int yk_get_key_vid_pid(YK_KEY *yk, int *vid, int *pid);
/* Get the YK4 capabilities */
int yk_get_capabilities(YK_KEY *yk, uint8_t slot, unsigned int flags,
			unsigned char *capabilities, unsigned int *len);
/* Set the device info (TLV string) */
int yk_write_device_info(YK_KEY *yk, unsigned char *buf, unsigned int len);


/*************************************************************************
 *
 * Error handling fuctions
 *
 ****/
extern int * _yk_errno_location(void);
#define yk_errno (*_yk_errno_location())
const char *yk_strerror(int errnum);
/* The following function is only useful if yk_errno == YK_EUSBERR and
   no other USB-related operations have been performed since the time of
   error.  */
const char *yk_usb_strerror(void);


/* Swaps the two bytes between little and big endian on big endian machines */
extern uint16_t yk_endian_swap_16(uint16_t x);

#define YK_EUSBERR	0x01	/* USB error reporting should be used */
#define YK_EWRONGSIZ	0x02
#define YK_EWRITEERR	0x03
#define YK_ETIMEOUT	0x04
#define YK_ENOKEY	0x05
#define YK_EFIRMWARE	0x06
#define YK_ENOMEM	0x07
#define YK_ENOSTATUS	0x08
#define YK_ENOTYETIMPL	0x09
#define YK_ECHECKSUM	0x0a	/* checksum validation failed */
#define YK_EWOULDBLOCK	0x0b	/* operation would block */
#define YK_EINVALIDCMD	0x0c	/* supplied command is invalid for this operation */
#define YK_EMORETHANONE	0x0d    /* expected to find only one key but found more */
#define YK_ENODATA	0x0e	/* no data was returned from a read */

/* Flags for response reading. Use high numbers to not exclude the possibility
 * to combine these with for example SLOT commands from ykdef.h in the future.
 */
#define YK_FLAG_MAYBLOCK	0x01 << 16

#define YK_CRC_OK_RESIDUAL	0xf0b8

# ifdef __cplusplus
}
# endif

#endif	/* __YKCORE_H_INCLUDED__ */
