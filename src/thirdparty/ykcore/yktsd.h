/* yktsd.h -*- mode:C; c-file-style: "gnu" -*- */
/* Note: this file is copied from Levitte Programming's LPlib and reworked
   for ykcore */
/*
 * Copyright (c) 2008-2012 Yubico AB
 * Copyright (c) 2010 Simon Josefsson <simon@josefsson.org>
 * Copyright (c) 2003, 2004 Richard Levitte <richard@levitte.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
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

#ifndef YKTSD_H
#define YKTSD_H

/* Define thread-specific data primitives */
#if defined _WIN32
#include <windows.h>
#include <errno.h>
#define yk__TSD_TYPE			DWORD
#define yk__TSD_ALLOC(key,nop)		((key = TlsAlloc()) == TLS_OUT_OF_INDEXES ? EAGAIN : 0)
#define yk__TSD_FREE(key)		(!TlsFree(key))
#define yk__TSD_SET(key,value)		(!TlsSetValue(key,value))
#define yk__TSD_GET(key)		TlsGetValue(key)
#else
#include <pthread.h>
#define yk__TSD_TYPE			pthread_key_t
#define yk__TSD_ALLOC(key,destr)	pthread_key_create(&key, destr)
#define yk__TSD_FREE(key)		pthread_key_delete(key)
#define yk__TSD_SET(key,value)		pthread_setspecific(key,(void *)value)
#define yk__TSD_GET(key)		pthread_getspecific(key)
#endif

/* Define the high-level macros that we use.  */
#define YK_TSD_METADATA(x)		yk__tsd_##x
#define YK_DEFINE_TSD_METADATA(x)	static yk__TSD_TYPE YK_TSD_METADATA(x)
#define YK_TSD_INIT(x,destr)		yk__TSD_ALLOC(YK_TSD_METADATA(x),destr)
#define YK_TSD_DESTROY(x)		yk__TSD_FREE(YK_TSD_METADATA(x))
#define YK_TSD_SET(x,value)		yk__TSD_SET(YK_TSD_METADATA(x),value)
#define YK_TSD_GET(type,x)		(type)yk__TSD_GET(YK_TSD_METADATA(x))

#endif
