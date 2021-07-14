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

#include "ykcore_lcl.h"
#include "ykcore_backend.h"
#include "yktsd.h"
#include "ykbzero.h"

/* To get modhex and crc16 */
#include "yubikey.h"

#include <stdio.h>
#ifndef _WIN32
#include <time.h>
#define Sleep(milliseconds) \
    struct timespec ts; \
    ts.tv_sec = milliseconds / (unsigned int) 1e3; \
    ts.tv_nsec = (milliseconds % (unsigned int) 1e3) * 1e6; \
    nanosleep(&ts, NULL);
#endif

#ifdef YK_DEBUG
#define _yk_hexdump(buffer, size) \
	do { \
		unsigned char *p = buffer; \
		int i; \
		fprintf(stderr, "%25s: ", __func__); \
		for(i = 0; i < size; i++) { \
			fprintf(stderr, "%02x ", *p++); \
		} \
		fprintf(stderr, "\n"); \
	} while(0)
#endif

/*
 * Yubikey low-level interface section 2.4 (Report arbitration polling) specifies
 * a 600 ms timeout for a Yubikey to process something written to it.
 * Where can that document be found?
 * It has been discovered that for swap 600 is not enough, swapping can worst
 * case take 920 ms, which we then add 25% to for safety margin, arriving at
 * 1150 ms.
 */
#define WAIT_FOR_WRITE_FLAG	1150

int yk_init(void)
{
	return _ykusb_start();
}

int yk_release(void)
{
	return _ykusb_stop();
}

YK_KEY *yk_open_first_key(void)
{
	return yk_open_key(0);
}

YK_KEY *yk_open_key_vid_pid(const int* vids, size_t vids_len, const int* pids, size_t pids_len, int index)
{
	YK_KEY *yk = _ykusb_open_device(vids, vids_len, pids, pids_len, index);
	int rc = yk_errno;

	if (yk) {
		YK_STATUS st;

		if (!yk_get_status(yk, &st)) {
			rc = yk_errno;
			yk_close_key(yk);
			yk = NULL;
		}
	}
	yk_errno = rc;
	return yk;
}

static const int yubico_vids[] = {YUBICO_VID};
static const int yubico_pids[] = {YUBIKEY_PID, NEO_OTP_PID, NEO_OTP_CCID_PID,
	NEO_OTP_U2F_PID, NEO_OTP_U2F_CCID_PID, YK4_OTP_PID,
	YK4_OTP_U2F_PID, YK4_OTP_CCID_PID, YK4_OTP_U2F_CCID_PID,
	PLUS_U2F_OTP_PID};

YK_KEY *yk_open_key(int index)
{
	return yk_open_key_vid_pid(yubico_vids, sizeof(yubico_vids) / sizeof(yubico_vids[0]),
                               yubico_pids, sizeof(yubico_pids) / sizeof(yubico_pids[0]),
                               index);
}

int yk_close_key(YK_KEY *yk)
{
	return _ykusb_close_device(yk);
}

int yk_check_firmware_version(YK_KEY *k)
{
	YK_STATUS st;

	if (!yk_get_status(k, &st))
		return 0;

	return yk_check_firmware_version2(&st);
}


int yk_check_firmware_version2(YK_STATUS *st)
{
    (void)st;
	return 1;
}

int yk_get_status(YK_KEY *k, YK_STATUS *status)
{
	unsigned int status_count = 0;

	if (!yk_read_from_key(k, 0, status, sizeof(YK_STATUS), &status_count))
		return 0;

	if (status_count != sizeof(YK_STATUS)) {
		yk_errno = YK_EWRONGSIZ;
		return 0;
	}

	status->touchLevel = yk_endian_swap_16(status->touchLevel);

	return 1;
}

/* Read the factory programmed serial number from a YubiKey.
 * The possibility to retreive the serial number might be disabled
 * using configuration, so it should not be considered a fatal error
 * to not be able to read the serial number using this function.
 *
 * Serial number reading might also be configured to require user
 * interaction (YubiKey button press) on startup, in which case flags
 * might have to have YK_FLAG_MAYBLOCK set - haven't tried that.
 *
 * The slot parameter is here for future purposes only.
 */
int yk_get_serial(YK_KEY *yk, uint8_t slot, unsigned int flags, unsigned int *serial)
{
	unsigned char buf[FEATURE_RPT_SIZE * 2];
	unsigned int response_len = 0;
	unsigned int expect_bytes = 0;

	memset(buf, 0, sizeof(buf));

	if (!yk_write_to_key(yk, SLOT_DEVICE_SERIAL, &buf, 0))
		return 0;

	expect_bytes = 4;

	if (! yk_read_response_from_key(yk, slot, flags,
					&buf, sizeof(buf),
					expect_bytes,
					&response_len))
		return 0;

	/* Serial number is stored in big endian byte order, despite
	 * everything else in the YubiKey being little endian - for
	 * some good reason I don't remember.
	 */
	*serial =
		(buf[0] << 24) +
		(buf[1] << 16) +
		(buf[2] << 8) +
		(buf[3]);

	return 1;
}

int yk_get_capabilities(YK_KEY *yk, uint8_t slot, unsigned int flags,
		unsigned char *capabilities, unsigned int *len)
{
	unsigned int response_len = 0;

	if (!yk_write_to_key(yk, SLOT_YK4_CAPABILITIES, capabilities, 0))
		return 0;

	if (! yk_read_response_from_key(yk, slot, flags,
					capabilities, *len, 0, /* we have no idea how much data we'll get */
					&response_len))
		return 0;

	/* the first data of the capabilities string is the length */
	response_len = capabilities[0];
	response_len++;

	/* validate the length we got back from the hardware */
	if (response_len > *len) {
		yk_errno = YK_EWRONGSIZ;
		return 0;
	}

	*len = response_len;
	return 1;
}

static int _yk_write(YK_KEY *yk, uint8_t yk_cmd, unsigned char *buf, size_t len)
{
	YK_STATUS stat;
	int seq;

	/* Get current sequence # from status block */

	if (!yk_get_status(yk, &stat /*, 0*/))
		return 0;

	seq = stat.pgmSeq;

	/* Write to Yubikey */
	if (!yk_write_to_key(yk, yk_cmd, buf, len))
		return 0;

	/* When the Yubikey clears the SLOT_WRITE_FLAG, it has processed the last write.
	 * This wait can't be done in yk_write_to_key since some users of that function
	 * want to get the bytes in the status message, but when writing configuration
	 * we don't expect any data back.
	 */
	if(!yk_wait_for_key_status(yk, yk_cmd, 0, WAIT_FOR_WRITE_FLAG, false, SLOT_WRITE_FLAG, NULL))
		return 0;

	/* Verify update */

	if (!yk_get_status(yk, &stat /*, 0*/))
		return 0;

	yk_errno = YK_EWRITEERR;

	/* when both configurations from a YubiKey is erased it will return
	 * pgmSeq 0, if one is still configured after an erase pgmSeq is
	 * counted up as usual. */
	if((stat.touchLevel & (CONFIG1_VALID | CONFIG2_VALID)) == 0 && stat.pgmSeq == 0) {
		return 1;
	}
	return stat.pgmSeq != seq;
}

int yk_write_device_info(YK_KEY *yk, unsigned char *buf, unsigned int len)
{
	return _yk_write(yk, SLOT_YK4_SET_DEVICE_INFO, buf, len);
}


int yk_write_command(YK_KEY *yk, YK_CONFIG *cfg, uint8_t command,
		    unsigned char *acc_code)
{
	int ret;
	unsigned char buf[sizeof(YK_CONFIG) + ACC_CODE_SIZE];

	/* Update checksum and insert config block in buffer if present */

	memset(buf, 0, sizeof(buf));

	if (cfg) {
		cfg->crc = ~yubikey_crc16 ((unsigned char *) cfg,
					   sizeof(YK_CONFIG) - sizeof(cfg->crc));
		cfg->crc = yk_endian_swap_16(cfg->crc);
		memcpy(buf, cfg, sizeof(YK_CONFIG));
	}

	/* Append current access code if present */

	if (acc_code)
		memcpy(buf + sizeof(YK_CONFIG), acc_code, ACC_CODE_SIZE);

	ret = _yk_write(yk, command, buf, sizeof(buf));
	insecure_memzero(buf, sizeof(buf));
	return ret;
}

int yk_write_config(YK_KEY *yk, YK_CONFIG *cfg, int confnum,
		    unsigned char *acc_code)
{
	uint8_t command;
	switch(confnum) {
	case 1:
		command = SLOT_CONFIG;
		break;
	case 2:
		command = SLOT_CONFIG2;
		break;
	default:
		yk_errno = YK_EINVALIDCMD;
		return 0;
	}
	if(!yk_write_command(yk, cfg, command, acc_code)) {
		return 0;
	}
	return 1;
}

int yk_write_ndef(YK_KEY *yk, YK_NDEF *ndef)
{
	/* just wrap yk_write_ndef2() with confnum 1 */
	return yk_write_ndef2(yk, ndef, 1);
}

int yk_write_ndef2(YK_KEY *yk, YK_NDEF *ndef, int confnum)
{
	unsigned char buf[sizeof(YK_NDEF)];
	uint8_t command;

	switch(confnum) {
		case 1:
			command = SLOT_NDEF;
			break;
		case 2:
			command = SLOT_NDEF2;
			break;
		default:
			yk_errno = YK_EINVALIDCMD;
			return 0;
	}

	/* Insert config block in buffer */

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ndef, sizeof(YK_NDEF));

	return _yk_write(yk, command, buf, sizeof(YK_NDEF));
}

int yk_write_device_config(YK_KEY *yk, YK_DEVICE_CONFIG *device_config)
{
	unsigned char buf[sizeof(YK_DEVICE_CONFIG)];

	memset(buf, 0, sizeof(buf));
	memcpy(buf, device_config, sizeof(YK_DEVICE_CONFIG));

	return _yk_write(yk, SLOT_DEVICE_CONFIG, buf, sizeof(YK_DEVICE_CONFIG));
}

int yk_write_scan_map(YK_KEY *yk, unsigned char *scan_map)
{
	return _yk_write(yk, SLOT_SCAN_MAP, scan_map, strlen(SCAN_MAP));
}

/*
 * This function is for doing HMAC-SHA1 or Yubico challenge-response with a key.
 */
int yk_challenge_response(YK_KEY *yk, uint8_t yk_cmd, int may_block,
		unsigned int challenge_len, const unsigned char *challenge,
		unsigned int response_len, unsigned char *response)
{
	unsigned int flags = 0;
	unsigned int bytes_read = 0;
	unsigned int expect_bytes = 0;

	switch(yk_cmd) {
	case SLOT_CHAL_HMAC1:
	case SLOT_CHAL_HMAC2:
		expect_bytes = 20;
		break;
	case SLOT_CHAL_OTP1:
	case SLOT_CHAL_OTP2:
		expect_bytes = 16;
		break;
	default:
		yk_errno = YK_EINVALIDCMD;
		return 0;
	}

	if (may_block)
		flags |= YK_FLAG_MAYBLOCK;

	if (! yk_write_to_key(yk, yk_cmd, challenge, challenge_len)) {
		return 0;
	}

	if (! yk_read_response_from_key(yk, yk_cmd, flags,
				response, response_len,
				expect_bytes,
				&bytes_read)) {
		return 0;
	}

	return 1;
}

int * _yk_errno_location(void)
{
	static int tsd_init = 0;
	static int nothread_errno = 0;
	YK_DEFINE_TSD_METADATA(errno_key);
	int rc = 0;

	if (tsd_init == 0) {
		if ((rc = YK_TSD_INIT(errno_key, free)) == 0) {
			tsd_init = 1;
		} else {
			tsd_init = -1;
		}
	}

	if(YK_TSD_GET(int *, errno_key) == NULL) {
		void *p = calloc(1, sizeof(int));
		if (!p) {
			tsd_init = -1;
		} else {
            (void)!YK_TSD_SET(errno_key, p);
		}
	}
	if (tsd_init == 1) {
		return YK_TSD_GET(int *, errno_key);
	}
	return &nothread_errno;
}

static const char *errtext[] = {
	"",
	"USB error",
	"wrong size",
	"write error",
	"timeout",
	"no yubikey present",
	"unsupported firmware version",
	"out of memory",
	"no status structure given",
	"not yet implemented",
	"checksum mismatch",
	"operation would block",
	"invalid command for operation",
	"expected only one YubiKey but several present",
	"no data returned from device",
};
const char *yk_strerror(int errnum)
{
	if (errnum < (int)(sizeof(errtext)/sizeof(errtext[0])))
		return errtext[errnum];
	return NULL;
}
const char *yk_usb_strerror(void)
{
	return _ykusb_strerror();
}

/* This function would've been better named 'yk_read_status_from_key'. Because
 * it disregards the first byte in each feature report, it can't be used to read
 * generic feature reports from the Yubikey, and this behaviour can't be changed
 * without breaking compatibility with existing programs.
 *
 * See yk_read_response_from_key() for a generic purpose data reading function.
 *
 * The slot parameter is here for future purposes only.
 */
int yk_read_from_key(YK_KEY *yk, uint8_t slot,
		     void *buf, unsigned int bufsize, unsigned int *bufcount)
{
    (void)slot;
	unsigned char data[FEATURE_RPT_SIZE];

	if (bufsize > FEATURE_RPT_SIZE - 1) {
		yk_errno = YK_EWRONGSIZ;
		return 0;
	}

	memset(data, 0, sizeof(data));

	if (!_ykusb_read(yk, REPORT_TYPE_FEATURE, 0, (char *)data, FEATURE_RPT_SIZE))
		return 0;

	/* This makes it apparent that there's some mysterious value in
	   the first byte...  I wonder what...  /Richard Levitte */
	memcpy(buf, data + 1, bufsize);
	*bufcount = bufsize;

	return 1;
}

/* Wait for the Yubikey to either set or clear (controlled by the boolean logic_and)
 * the bits in mask.
 *
 * The slot parameter is here for future purposes only.
 */
int yk_wait_for_key_status(YK_KEY *yk, uint8_t slot, unsigned int flags,
			   unsigned int max_time_ms,
			   bool logic_and, unsigned char mask,
			   unsigned char *last_data)
{
	unsigned char data[FEATURE_RPT_SIZE];

	unsigned int sleepval = 1;
	unsigned int slept_time = 0;
	int blocking = 0;

	/* Non-zero slot breaks on Windows (libusb-1.0.8-win32), while working fine
	 * on Linux (and probably MacOS X).
	 *
	 * The YubiKey doesn't support per-slot status anyways at the moment (2.2),
	 * so we just set it to 0 (meaning slot 1).
	 */
	slot = 0;

	while (slept_time < max_time_ms) {
		Sleep(sleepval);
		slept_time += sleepval;
		/* exponential backoff, up to 500 ms */
		sleepval *= 2;
		if (sleepval > 500)
			sleepval = 500;

		/* Read a status report from the key */
		memset(data, 0, sizeof(data));
		if (!_ykusb_read(yk, REPORT_TYPE_FEATURE, slot, (char *) &data, FEATURE_RPT_SIZE))
			return 0;
#ifdef YK_DEBUG
		_yk_hexdump(data, FEATURE_RPT_SIZE);
#endif

		if (last_data != NULL)
			memcpy(last_data, data, sizeof(data));

		/* The status byte from the key is now in last byte of data */
		if (logic_and) {
			/* Check if Yubikey has SET the bit(s) in mask */
			if ((data[FEATURE_RPT_SIZE - 1] & mask) == mask) {
				return 1;
			}
		} else {
			/* Check if Yubikey has CLEARED the bit(s) in mask */
			if (! (data[FEATURE_RPT_SIZE - 1] & mask)) {
				return 1;
			}
		}

		/* Check if Yubikey says it will wait for user interaction */
		if ((data[FEATURE_RPT_SIZE - 1] & RESP_TIMEOUT_WAIT_FLAG) == RESP_TIMEOUT_WAIT_FLAG) {
			if ((flags & YK_FLAG_MAYBLOCK) == YK_FLAG_MAYBLOCK) {
				if (! blocking) {
					/* Extend timeout first time we see RESP_TIMEOUT_WAIT_FLAG. */
					blocking = 1;
					max_time_ms += 256 * 1000;
				}
			} else {
				/* Reset read mode of Yubikey before aborting. */
				yk_force_key_update(yk);
				yk_errno = YK_EWOULDBLOCK;
				return 0;
			}
		} else {
			if (blocking) {
				/* YubiKey timed out waiting for user interaction */
				break;
			}
		}
	}

	yk_errno = YK_ETIMEOUT;
	return 0;
}

/* Read one or more feature reports from a Yubikey and put them together.
 *
 * Bufsize must be able to hold at least 2 more bytes than you are expecting
 * (the CRC), but since all read requests return 7 bytes of data bufsize needs
 * to be up to 7 bytes more than you expect.
 *
 * If the key returns more data than bufsize, we fail and set yk_errno to
 * YK_EWRONGSIZ. If that happens there will be partial data in buf.
 *
 * If we read a response from a Yubikey that is configured to block and wait for
 * a button press (in challenge response), this function will abort unless
 * flags contain YK_FLAG_MAYBLOCK, in which case it might take up to 15 seconds
 * for this function to return.
 *
 * The slot parameter is here for future purposes only.
 */
int yk_read_response_from_key(YK_KEY *yk, uint8_t slot, unsigned int flags,
			      void *buf, unsigned int bufsize, unsigned int expect_bytes,
			      unsigned int *bytes_read)
{
	unsigned char data[FEATURE_RPT_SIZE];
	memset(data, 0, sizeof(data));

	memset(buf, 0, bufsize);
	*bytes_read = 0;

#ifdef YK_DEBUG
	fprintf(stderr, "YK_DEBUG: Read %i bytes from YubiKey :\n", expect_bytes);
#endif
	/* Wait for the key to turn on RESP_PENDING_FLAG */
	if (! yk_wait_for_key_status(yk, slot, flags, 1000, true, RESP_PENDING_FLAG, (unsigned char *) &data))
		return 0;

	/* The first part of the response was read by yk_wait_for_key_status(). We need
	 * to copy it to buf.
	 */
	memcpy((char*)buf + *bytes_read, data, sizeof(data) - 1);
	*bytes_read += sizeof(data) - 1;

	while (*bytes_read + FEATURE_RPT_SIZE <= bufsize) {
		memset(data, 0, sizeof(data));

		if (!_ykusb_read(yk, REPORT_TYPE_FEATURE, 0, (char *)data, FEATURE_RPT_SIZE))
			return 0;
#ifdef YK_DEBUG
		_yk_hexdump(data, FEATURE_RPT_SIZE);
#endif
		if (data[FEATURE_RPT_SIZE - 1] & RESP_PENDING_FLAG) {
			/* The lower five bits of the status byte has the response sequence
			 * number. If that gets reset to zero we are done.
			 */
			if ((data[FEATURE_RPT_SIZE - 1] & 31) == 0) {
				if (expect_bytes > 0) {
					/* Size of response is known. Verify CRC. */
					expect_bytes += 2;
					int crc = yubikey_crc16(buf, expect_bytes);
					if (crc != YK_CRC_OK_RESIDUAL) {
						yk_errno = YK_ECHECKSUM;
						return 0;
					}

					/* since we get data in chunks of 7 we need to round expect bytes out to the closest higher multiple of 7 */
					if(expect_bytes % 7 != 0) {
						expect_bytes += 7 - (expect_bytes % 7);
					}

					if (*bytes_read != expect_bytes) {
						yk_errno = YK_EWRONGSIZ;
						return 0;
					}
				}

				/* Reset read mode of Yubikey before returning. */
				yk_force_key_update(yk);

				return 1;
			}

			memcpy((char*)buf + *bytes_read, data, sizeof(data) - 1);
			*bytes_read += sizeof(data) - 1;
		} else {
			/* Reset read mode of Yubikey before returning. */
			yk_force_key_update(yk);

			return 0;
		}
	}

	/* We're out of buffer space, abort reading */
	yk_force_key_update(yk);

	yk_errno = YK_EWRONGSIZ;
	return 0;
}

/*
 * Send something to the YubiKey. The command, as well as the slot, is
 * given in the 'slot' parameter (e.g. SLOT_CHAL_HMAC2 to send a HMAC-SHA1
 * challenge to slot 2).
 */
int yk_write_to_key(YK_KEY *yk, uint8_t slot, const void *buf, int bufcount)
{
	YK_FRAME frame;
	unsigned char repbuf[FEATURE_RPT_SIZE];
	int i, seq;
	int ret = 0;
	unsigned char *ptr, *end;

	if (bufcount > (int)sizeof(frame.payload)) {
		yk_errno = YK_EWRONGSIZ;
		return 0;
	}

	/* Insert data and set slot # */

	memset(&frame, 0, sizeof(frame));
	memcpy(frame.payload, buf, bufcount);
	frame.slot = slot;

	/* Append slot checksum */

	i = yubikey_crc16 (frame.payload, sizeof(frame.payload));
	frame.crc = yk_endian_swap_16(i);

	/* Chop up the data into parts that fits into the payload of a
	   feature report. Set the sequence number | 0x80 in the end
	   of the feature report. When the Yubikey has processed it,
	   it will clear this byte, signaling that the next part can be
	   sent */

	ptr = (unsigned char *) &frame;
	end = (unsigned char *) &frame + sizeof(frame);

#ifdef YK_DEBUG
	fprintf(stderr, "YK_DEBUG: Write %i bytes to YubiKey :\n", bufcount);
#endif
	for (seq = 0; ptr < end; seq++) {
		int all_zeros = 1;
		/* Ignore parts that are all zeroes except first and last
		   to speed up the transfer */

		for (i = 0; i < (FEATURE_RPT_SIZE - 1); i++) {
			if ((repbuf[i] = *ptr++)) all_zeros = 0;
		}
		if (all_zeros && (seq > 0) && (ptr < end))
			continue;

		/* sequence number goes into lower bits of last byte */
		repbuf[i] = seq | SLOT_WRITE_FLAG;

		/* When the Yubikey clears the SLOT_WRITE_FLAG, the
		 * next part can be sent.
		 */
		if (! yk_wait_for_key_status(yk, slot, 0, WAIT_FOR_WRITE_FLAG,
					     false, SLOT_WRITE_FLAG, NULL))
			goto end;
#ifdef YK_DEBUG
		_yk_hexdump(repbuf, FEATURE_RPT_SIZE);
#endif
		if (!_ykusb_write(yk, REPORT_TYPE_FEATURE, 0,
				  (char *)repbuf, FEATURE_RPT_SIZE))
			goto end;
	}

	ret = 1;
end:
	insecure_memzero(&frame, sizeof(YK_FRAME));
	insecure_memzero(repbuf, sizeof(repbuf));
	return ret;
}

int yk_force_key_update(YK_KEY *yk)
{
	unsigned char buf[FEATURE_RPT_SIZE];

	memset(buf, 0, sizeof(buf));
	buf[FEATURE_RPT_SIZE - 1] = DUMMY_REPORT_WRITE; /* Invalid sequence = update only */
	if (!_ykusb_write(yk, REPORT_TYPE_FEATURE, 0, (char *)buf, FEATURE_RPT_SIZE))
		return 0;

	return 1;
}

int yk_get_key_vid_pid(YK_KEY *yk, int *vid, int *pid) {
	return _ykusb_get_vid_pid(yk, vid, pid);
}

uint16_t yk_endian_swap_16(uint16_t x)
{
	static int testflag = -1;

	if (testflag == -1) {
		uint16_t testword = 0x0102;
		unsigned char *testchars = (unsigned char *)&testword;
		if (*testchars == '\1')
			testflag = 1; /* Big endian arch, swap needed */
		else
			testflag = 0; /* Little endian arch, no swap needed */
	}

	if (testflag)
		x = (x >> 8) | ((x & 0xff) << 8);

	return x;
}
