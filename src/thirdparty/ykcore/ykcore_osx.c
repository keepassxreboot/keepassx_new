/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2014 Yubico AB
 * Copyright (c) 2009 Christer Kaivo-oja <christer.kaivooja@gmail.com>
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

#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ykcore_backend.h"

#define	FEATURE_RPT_SIZE		8
#define FIDO_HID_USAGE_PAGE		0xF1D0
#define FIDO_U2F_DEVICE_USAGE	0x01

static IOHIDManagerRef ykosxManager = NULL;
static IOReturn _ykusb_IOReturn = 0;

int _ykusb_start(void)
{
	ykosxManager = IOHIDManagerCreate( kCFAllocatorDefault, 0L );

	return 1;
}

int _ykusb_stop(void)
{
	if (ykosxManager != NULL) {
		CFRelease(ykosxManager);
		ykosxManager = NULL;
		return 1;
	}

	yk_errno = YK_EUSBERR;
	return 0;
}

static void _ykosx_CopyToCFArray(const void *value, void *context)
{
	CFArrayAppendValue( ( CFMutableArrayRef ) context, value );
}

static int _ykosx_getIntProperty( IOHIDDeviceRef dev, CFStringRef key )
{
	int result = 0;
	CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty( dev, key );
	if ( tCFTypeRef ) {
		if ( CFNumberGetTypeID( ) == CFGetTypeID( tCFTypeRef ) ) {
			CFNumberGetValue( ( CFNumberRef ) tCFTypeRef, kCFNumberSInt32Type, &result );
		}
	}
	return result;
}

static IOHIDDeviceRef _ykosx_getHIDDeviceMatching(CFArrayRef devices, 
                                                  int primaryUsagePage, 
                                                  int primaryUsage,
                                                  const int *productIDs,
                                                  size_t productIDsCount,
                                                  int index)
{
    IOHIDDeviceRef 	matchingDevice = NULL;
    size_t          cnt, i, j;
    int			    found = 0;
	
	cnt = CFArrayGetCount( devices );
	
	for(i = 0; i < cnt; ++i) {
		IOHIDDeviceRef dev = (IOHIDDeviceRef)CFArrayGetValueAtIndex( devices, i );
		const int usagePage = _ykosx_getIntProperty( dev, CFSTR( kIOHIDPrimaryUsagePageKey ));
		const int usage = _ykosx_getIntProperty( dev, CFSTR( kIOHIDPrimaryUsageKey ));
        const int devProductId = _ykosx_getIntProperty( dev, CFSTR( kIOHIDProductIDKey ));
		
		if (usagePage != primaryUsagePage || usage != primaryUsage) {
			continue;
		}
		
		for(j = 0; j < productIDsCount; j++) {
			if(productIDs[j] == devProductId) {
				found++;
				if(found - 1 == index) {
					matchingDevice = dev;
					break;
				}
			}
		}
	}
	
	return matchingDevice;
}

void *_ykusb_open_device(const int* vendor_ids, size_t vids_len, const int *product_ids, size_t pids_len, int index)
{
    (void) vendor_ids;
    (void) vids_len;

	IOHIDDeviceRef yk = NULL;

	int rc = YK_ENOKEY;

	IOHIDManagerSetDeviceMatchingMultiple( ykosxManager, NULL );

	CFSetRef devSet = IOHIDManagerCopyDevices( ykosxManager );

	if ( devSet ) {
		CFMutableArrayRef array = CFArrayCreateMutable( kCFAllocatorDefault, 0, NULL );

		CFSetApplyFunction( devSet, _ykosx_CopyToCFArray, array );
		
		/* ensure that we are attempting to open the FIDO interface instead
		   of the keyboard interface. macOS requires explicit user
		   authorization before we are able to open HID devices such as
		   keyboards and mice, while it is not required for FIDO devices
		   that communicate over HID. */
		yk = _ykosx_getHIDDeviceMatching(
			array,					// devices
			FIDO_HID_USAGE_PAGE,	// primaryUsagePage
			FIDO_U2F_DEVICE_USAGE,	// primaryUsage
			product_ids,			// productIDs
			pids_len,				// productIDsCount
			index					// index
		);
		
		if(yk == NULL) {
			/* fallback to the keyboard device if it is present. */
			yk = _ykosx_getHIDDeviceMatching(
				array,						// devices
				kHIDPage_GenericDesktop,	// primaryUsagePage
				kHIDUsage_GD_Keyboard,		// primaryUsage
				product_ids,				// productIDs
				pids_len,					// productIDsCount
				index						// index
			);
		}

		/* this is a workaround for a memory leak in IOHIDManagerCopyDevices() in 10.8 */
		IOHIDManagerScheduleWithRunLoop( ykosxManager, CFRunLoopGetCurrent( ), kCFRunLoopDefaultMode );
		IOHIDManagerUnscheduleFromRunLoop( ykosxManager, CFRunLoopGetCurrent( ), kCFRunLoopDefaultMode );

		CFRelease( array );
		CFRelease( devSet );
	}

	if (yk) {
		CFRetain(yk);
		_ykusb_IOReturn = IOHIDDeviceOpen( yk, 0L );

		if ( _ykusb_IOReturn != kIOReturnSuccess ) {
			CFRelease(yk);
			rc = YK_EUSBERR;
			goto error;
		}

		return (void *)yk;
	}

error:
	yk_errno = rc;
	return 0;
}

int _ykusb_close_device(void *dev)
{
	_ykusb_IOReturn = IOHIDDeviceClose( dev, 0L );
	CFRelease(dev);

	if ( _ykusb_IOReturn == kIOReturnSuccess )
		return 1;

	yk_errno = YK_EUSBERR;
	return 0;
}

int _ykusb_read(void *dev, int report_type, int report_number,
		char *buffer, int size)
{
	CFIndex sizecf = (CFIndex)size;

	if (report_type != REPORT_TYPE_FEATURE)
	{
		yk_errno = YK_ENOTYETIMPL;
		return 0;
	}

	_ykusb_IOReturn = IOHIDDeviceGetReport( dev, kIOHIDReportTypeFeature, report_number, (uint8_t *)buffer, (CFIndex *) &sizecf );

	if ( _ykusb_IOReturn != kIOReturnSuccess )
	{
		yk_errno = YK_EUSBERR;
		return 0;
	}

	if(sizecf == 0)
		yk_errno = YK_ENODATA;

	return (int)sizecf;
}

int _ykusb_write(void *dev, int report_type, int report_number,
		char *buffer, int size)
{
	if (report_type != REPORT_TYPE_FEATURE)
	{
		yk_errno = YK_ENOTYETIMPL;
		return 0;
	}

	_ykusb_IOReturn = IOHIDDeviceSetReport( dev, kIOHIDReportTypeFeature, report_number, (unsigned char *)buffer, size);

	if ( _ykusb_IOReturn != kIOReturnSuccess )
	{
		yk_errno = YK_EUSBERR;
		return 0;
	}

	return 1;
}

int _ykusb_get_vid_pid(void *yk, int *vid, int *pid) {
	IOHIDDeviceRef dev = (IOHIDDeviceRef)yk;
	*vid = _ykosx_getIntProperty( dev, CFSTR( kIOHIDVendorIDKey ));
	*pid = _ykosx_getIntProperty( dev, CFSTR( kIOHIDProductIDKey ));
	return 1;
}

const char *_ykusb_strerror()
{
	switch (_ykusb_IOReturn) {
		case kIOReturnSuccess:
			return "kIOReturnSuccess";
		case kIOReturnNotOpen:
			return "kIOReturnNotOpen";
		case kIOReturnNoDevice:
			return "kIOReturnNoDevice";
		case kIOReturnExclusiveAccess:
			return "kIOReturnExclusiveAccess";
		case kIOReturnError:
			return "kIOReturnError";
		case kIOReturnBadArgument:
			return "kIOReturnBadArgument";
		case kIOReturnAborted:
			return "kIOReturnAborted";
		case kIOReturnNotResponding:
			return "kIOReturnNotResponding";
		case kIOReturnOverrun:
			return "kIOReturnOverrun";
		case kIOReturnCannotWire:
			return "kIOReturnCannotWire";
		default:
			return "unknown error";
	}
}
