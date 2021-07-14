/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2014 Yubico AB
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

#include "ykcore.h"
#include "ykdef.h"
#include "ykcore_backend.h"

#define INITGUID
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <ntddkbd.h>
#include <hidsdi.h>

int _ykusb_start(void)
{
	return 1;
}

int _ykusb_stop(void)
{
	return 1;
}

void * _ykusb_open_device(const int* vendor_ids, size_t vids_len, const int *product_ids, size_t pids_len, int index)
{
	HDEVINFO hi;
	SP_DEVICE_INTERFACE_DATA di;
	PSP_DEVICE_INTERFACE_DETAIL_DATA pi;
	int i;
	DWORD len, rc;
	HANDLE ret_handle = NULL;
	int found = 0;

	yk_errno = YK_EUSBERR;

	hi = SetupDiGetClassDevs(&GUID_DEVINTERFACE_KEYBOARD, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hi == INVALID_HANDLE_VALUE)
		return NULL;

	di.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	for (i = 0; i < 1000; i++) {
		if (!SetupDiEnumDeviceInterfaces(hi, 0, &GUID_DEVINTERFACE_KEYBOARD, i, &di))
			break;

		if (SetupDiGetDeviceInterfaceDetail(hi, &di, 0, 0, &len, 0) || GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			break;

		pi = malloc (len);
		if (!pi) {
			yk_errno = YK_ENOMEM;
			goto done;
		}
		pi->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

		rc = SetupDiGetDeviceInterfaceDetail(hi, &di, pi, len, &len, 0);
		if (rc) {
			HANDLE m_handle;
            HIDD_ATTRIBUTES devInfo;
			m_handle = CreateFile(pi->DevicePath, GENERIC_WRITE,
					      FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
			if (m_handle != INVALID_HANDLE_VALUE && HidD_GetAttributes(m_handle, &devInfo)) {
                for (size_t k = 0; k < vids_len; k++) {
                    bool vid_match = devInfo.VendorID == vendor_ids[k];
                    for (size_t j = 0; vid_match && j < pids_len; j++) {
                        if (devInfo.ProductID == product_ids[j] && ++found == index + 1) {
                            ret_handle = m_handle;
                            goto done;
                        }
                    }
                }
			}
            CloseHandle (m_handle);
		}
		free (pi);
	}

    // No key found
    yk_errno = YK_ENOKEY;

done:
	SetupDiDestroyDeviceInfoList(hi);
	return ret_handle;
}

int _ykusb_close_device(void *yk)
{
	HANDLE h = yk;

	CloseHandle(h);

	return 1;
}

#define EXPECT_SIZE 8
#define FEATURE_BUF_SIZE 9

int _ykusb_read(void *dev, int report_type, int report_number,
		char *buffer, int buffer_size)
{
    (void)report_type;
    (void)report_number;

	HANDLE h = dev;
	BYTE buf[FEATURE_BUF_SIZE];

	if (buffer_size != EXPECT_SIZE) {
		yk_errno = YK_EUSBERR;
		return 0;
	}

	memset(buf, 0, sizeof(buf));

	if (!HidD_GetFeature(h, buf, sizeof (buf))) {
		yk_errno = YK_EUSBERR;
		return 0;
	}

	memcpy (buffer, buf + 1, buffer_size);

	return buffer_size;
}

int _ykusb_write(void *dev, int report_type, int report_number,
		 char *buffer, int buffer_size)
{
    (void)report_type;
    (void)report_number;

    HANDLE h = dev;
	BYTE buf[FEATURE_BUF_SIZE];

	if (buffer_size != EXPECT_SIZE) {
		yk_errno = YK_EUSBERR;
		return 0;
	}

	buf[0] = 0;
	memcpy (buf + 1, buffer, buffer_size);

	if (!HidD_SetFeature(h, buf, sizeof (buf))) {
		yk_errno = YK_EUSBERR;
		return 0;
	}

	return 1;
}

int _ykusb_get_vid_pid(void *yk, int *vid, int *pid) {
	HIDD_ATTRIBUTES devInfo;
	int rc = HidD_GetAttributes(yk, &devInfo);
	if (rc) {
		*vid = devInfo.VendorID;
		*pid = devInfo.ProductID;
		return 1;
	}
	yk_errno = YK_EUSBERR;
	return 0;
}

const char *_ykusb_strerror(void)
{
	static char buf[1024];
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0,
			buf, sizeof(buf), NULL);
	return buf;
}
