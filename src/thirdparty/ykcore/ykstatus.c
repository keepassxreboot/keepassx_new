/* -*- mode:C; c-file-style: "bsd" -*- */
/*
 * Written by Richard Levitte <richard@levitte.org>
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

#include "ykcore_lcl.h"
#include "ykdef.h"
#include "ykstatus.h"

YK_STATUS *ykds_alloc(void)
{
	YK_STATUS *st = malloc(sizeof(YK_STATUS));
	if (!st) {
		yk_errno = YK_ENOMEM;
	}
	return st;
}

void ykds_free(YK_STATUS *st)
{
	free(st);
}

YK_STATUS *ykds_static(void)
{
	static YK_STATUS st;
	return &st;
}

extern int ykds_version_major(const YK_STATUS *st)
{
	if (st)
		return st->versionMajor;
	yk_errno = YK_ENOSTATUS;
	return 0;
}
extern int ykds_version_minor(const YK_STATUS *st)
{
	if (st)
		return st->versionMinor;
	yk_errno = YK_ENOSTATUS;
	return 0;
}
extern int ykds_version_build(const YK_STATUS *st)
{
	if (st)
		return st->versionBuild;
	yk_errno = YK_ENOSTATUS;
	return 0;
}
extern int ykds_pgm_seq(const YK_STATUS *st)
{
	if (st)
		return st->pgmSeq;
	yk_errno = YK_ENOSTATUS;
	return 0;
}
extern int ykds_touch_level(const YK_STATUS *st)
{
	if (st)
		return st->touchLevel;
	yk_errno = YK_ENOSTATUS;
	return 0;
}
