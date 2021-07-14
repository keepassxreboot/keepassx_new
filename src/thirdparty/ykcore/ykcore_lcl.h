/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2012 Yubico AB
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

#ifndef	__YKCORE_LCL_H_INCLUDED__
#define	__YKCORE_LCL_H_INCLUDED__

/* This is a hack to map official structure names (in ykcore.h) to
   internal ones (in ykdef.h) */
#define yk_key_st yubikey_st
#define yk_status_st status_st
#define yk_ticket_st ticket_st
#define yk_config_st config_st
#define yk_nav_st nav_st
#define yk_frame_st frame_st
#define yk_device_config_st device_config_st

#include "ykcore.h"
#include "ykdef.h"

/*************************************************************************
 **
 ** = = = = = = = = =   B I G   F A T   W A R N I N G   = = = = = = = = =
 **
 ** DO NOT USE THE FOLLOWING FUCTIONS DIRECTLY UNLESS YOU WRITE CORE ROUTINES!
 **
 ** These functions are declared here only to make sure they get defined
 ** correctly internally.
 **
 ** YOU HAVE BEEN WARNED!
 **
 ****/

/*************************************************************************
 *
 * Functions to send and receive data to/from the key.
 *
 ****/
extern int yk_read_from_key(YK_KEY *k, uint8_t slot,
			    void *buf, unsigned int bufsize,
			    unsigned int *bufcount);

#endif	/* __YKCORE_LCL_H_INCLUDED__ */
