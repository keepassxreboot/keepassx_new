/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Copyright (c) 2008-2019 Yubico AB
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

#ifndef	__YKBZERO_H_INCLUDED__
#define	__YKBZERO_H_INCLUDED__

#ifdef _WIN32
#include <windows.h>
#else
#include <string.h>
#endif

#if defined(_WIN32)
#define insecure_memzero(buf, len) SecureZeroMemory(buf, len)
#elif defined(HAVE_MEMSET_S)
#define insecure_memzero(buf, len) memset_s(buf, len, 0, len)
#elif defined(HAVE_EXPLICIT_BZERO)
#define insecure_memzero(buf, len) explicit_bzero(buf, len)
#elif defined(HAVE_EXPLICIT_MEMSET)
#define insecure_memzero(buf, len) explicit_memset(buf, 0, len)
#elif defined(HAVE_INLINE_ASM)
#define insecure_memzero(buf, len) do {                                 \
                memset(buf, 0, len);                                    \
                __asm__ __volatile__ ("" : : "r"(buf) : "memory");      \
        } while (0)
#else
#define insecure_memzero(buf, len) do {                                 \
                volatile unsigned char *volatile __buf_ =               \
                        (volatile unsigned char *volatile)buf;          \
                size_t __i_ = 0;                                        \
                while (__i_ < len) __buf_[__i_++] = 0;                  \
        } while (0)
#endif

#endif	/* __YKBZERO_H_INCLUDED__ */
