/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2014 Yubico AB
 * Copyright (c) 2009 Tollef Fog Heen <tfheen@err.no>
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

#include <libusb.h>
#include <stdio.h>
#include <string.h>

#include "ykcore.h"
#include "ykdef.h"
#include "ykcore_backend.h"

#define HID_GET_REPORT			0x01
#define HID_SET_REPORT			0x09

static int ykl_errno;
static int libusb_inited = 0;
static libusb_context *usb_ctx = NULL;

/*************************************************************************
 **  function _ykusb_write						**
 **  Set HID report							**
 **									**
 **  int _ykusb_write(YUBIKEY *yk, int report_type, int report_number,	**
 **			char *buffer, int size)				**
 **									**
 **  Where:								**
 **  "yk" is handle to open Yubikey					**
 **  "report_type" is HID report type (in, out or feature)		**
 **  "report_number" is report identifier				**
 **  "buffer" is pointer to in buffer					**
 **  "size" is size of the buffer					**
 **									**
 **  Returns: Nonzero if successful, zero otherwise			**
 **									**
 *************************************************************************/

int _ykusb_write(void *dev, int report_type, int report_number,
		 char *buffer, int size)
{
	ykl_errno = libusb_claim_interface((libusb_device_handle *)dev, 0);

	if (ykl_errno == 0) {
		int rc2;
		ykl_errno = libusb_control_transfer((libusb_device_handle *)dev,
					     LIBUSB_REQUEST_TYPE_CLASS |
					     LIBUSB_RECIPIENT_INTERFACE |
					     LIBUSB_ENDPOINT_OUT,
					     HID_SET_REPORT,
					     report_type << 8 | report_number, 0,
					     (unsigned char *)buffer, size,
					     1000);
		/* preserve a control message error over an interface
		   release one */
		rc2 = libusb_release_interface((libusb_device_handle *)dev, 0);
		if (ykl_errno > 0 && rc2 < 0)
			ykl_errno = rc2;
	}
	if (ykl_errno > 0)
		return 1;
	yk_errno = YK_EUSBERR;
	return 0;
}

/*************************************************************************
**  function _ykusb_read						**
**  Get HID report							**
**                                                                      **
**  int _ykusb_read(YUBIKEY *dev, int report_type, int report_number,	**
**		       char *buffer, int size)				**
**                                                                      **
**  Where:                                                              **
**  "dev" is handle to open Yubikey					**
**  "report_type" is HID report type (in, out or feature)		**
**  "report_number" is report identifier					**
**  "buffer" is pointer to in buffer					**
**  "size" is size of the buffer					**
**									**
**  Returns: Number of bytes read. Zero if failure			**
**                                                                      **
*************************************************************************/

int _ykusb_read(void *dev, int report_type, int report_number,
		char *buffer, int size)
{
	ykl_errno = libusb_claim_interface((libusb_device_handle *)dev, 0);

	if (ykl_errno == 0) {
		int rc2;
		ykl_errno = libusb_control_transfer((libusb_device_handle *)dev,
					     LIBUSB_REQUEST_TYPE_CLASS |
					     LIBUSB_RECIPIENT_INTERFACE | 
					     LIBUSB_ENDPOINT_IN,
					     HID_GET_REPORT,
					     report_type << 8 | report_number, 0,
					     (unsigned char *)buffer, size,
					     1000);
		/* preserve a control message error over an interface
		   release one */
		rc2 = libusb_release_interface((libusb_device_handle *)dev, 0);
		if (ykl_errno > 0 && rc2 < 0)
			ykl_errno = rc2;
	}
	if (ykl_errno > 0) {
		return ykl_errno;
	} else if(ykl_errno == 0) {
		yk_errno = YK_ENODATA;
	} else {
		yk_errno = YK_EUSBERR;
	}
	return 0;
}

int _ykusb_start(void)
{
	ykl_errno = libusb_init(&usb_ctx);
	if(ykl_errno) {
		yk_errno = YK_EUSBERR;
		return 0;
	}
	libusb_inited = 1;
	return 1;
}

extern int _ykusb_stop(void)
{
	if (libusb_inited == 1) {
		libusb_exit(usb_ctx);
		usb_ctx = NULL;
		libusb_inited = 0;
		return 1;
	}
	yk_errno = YK_EUSBERR;
	return 0;
}

void *_ykusb_open_device(const int* vendor_ids, size_t vids_len, const int *product_ids, size_t pids_len, int index)
{
	libusb_device *dev = NULL;
	libusb_device_handle *h = NULL;
	struct libusb_device_descriptor desc;
	libusb_device **list;
	ssize_t cnt = libusb_get_device_list(usb_ctx, &list);
	ssize_t i = 0;
	int rc = YK_ENOKEY;
	const int desired_cfg = 1;
	int found = 0;

	for (i = 0; i < cnt; i++) {
		ykl_errno = libusb_get_device_descriptor(list[i], &desc);
		if (ykl_errno != 0)
			goto done;
        size_t k;
        for (k = 0; k < vids_len; k++) {
            if (desc.idVendor == vendor_ids[k]) {
                size_t j;
                for (j = 0; j < pids_len; j++) {
                    if (desc.idProduct == product_ids[j]) {
                        found++;
                        if (found - 1 == index) {
                            dev = list[i];
                            goto found;
                        }
                    }
                }
            }
        }
	}

 found:
	if (dev) {
		int current_cfg;
		rc = YK_EUSBERR;
		ykl_errno = libusb_open(dev, &h);
		if (ykl_errno != 0)
			goto done;
		ykl_errno = libusb_kernel_driver_active(h, 0);
		if (ykl_errno == 1) {
			ykl_errno = libusb_detach_kernel_driver(h, 0);
			if (ykl_errno != 0)
				goto done;
		} else if (ykl_errno != 0)
			goto done;
		/* This is needed for yubikey-personalization to work inside virtualbox virtualization. */
		ykl_errno = libusb_get_configuration(h, &current_cfg);
		if (ykl_errno != 0)
			goto done;
		if (desired_cfg != current_cfg) {
			ykl_errno = libusb_set_configuration(h, desired_cfg);
			if (ykl_errno != 0)
				goto done;
		}
	}
 done:
	libusb_free_device_list(list, 1);
	if (h == NULL)
		yk_errno = rc;
	return h;
}

int _ykusb_close_device(void *yk)
{
	libusb_attach_kernel_driver(yk, 0);
	libusb_close((libusb_device_handle *) yk);
	return 1;
}

int _ykusb_get_vid_pid(void *yk, int *vid, int *pid)
{
	struct libusb_device_descriptor desc;
	libusb_device *dev = libusb_get_device(yk);
	int rc = libusb_get_device_descriptor(dev, &desc);

	if (rc == 0) {
		*vid = desc.idVendor;
		*pid = desc.idProduct;
		return 1;
	}
	yk_errno = YK_EUSBERR;
	return 0;
}

const char *_ykusb_strerror(void)
{
	static const char *buf;
	switch (ykl_errno) {
	case LIBUSB_SUCCESS:
		buf = "Success (no error)";
		break;
	case LIBUSB_ERROR_IO:
		buf = "Input/output error";
		break;
	case LIBUSB_ERROR_INVALID_PARAM:
		buf = "Invalid parameter";
		break;
	case LIBUSB_ERROR_ACCESS:
		buf = "Access denied (insufficient permissions)";
		break;
	case LIBUSB_ERROR_NO_DEVICE:
		buf = "No such device (it may have been disconnected)";
		break;
	case LIBUSB_ERROR_NOT_FOUND:
		buf = "Entity not found";
		break;
	case LIBUSB_ERROR_BUSY:
		buf = "Resource busy";
		break;
	case LIBUSB_ERROR_TIMEOUT:
		buf = "Operation timed out";
		break;
	case LIBUSB_ERROR_OVERFLOW:
		buf = "Overflow";
		break;
	case LIBUSB_ERROR_PIPE:
		buf = "Pipe error";
		break;
	case LIBUSB_ERROR_INTERRUPTED:
		buf = "System call interrupted (perhaps due to signal)";
		break;
	case LIBUSB_ERROR_NO_MEM:
		buf = "Insufficient memory";
		break;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		buf = "Operation not supported or unimplemented on this platform";
		break;
	case LIBUSB_ERROR_OTHER:
	default:
		buf = "Other/unknown error";
		break;
	}
	return buf;
}
