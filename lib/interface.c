/*-
 * Copyright (C) 2012 Gabor Kovesdan <gabor@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "compile.h"
#include "config.h"
#include "convert.h"
#include "heuristic.h"
#include "match.h"
#include "mregex.h"
#include "frec.h"
#include "wu-manber.h"

int
frec_regncomp(frec_t *preg, const char *regex, size_t n, int cflags)
{
	wchar_t *wregex;
	size_t wlen;
	int ret;

	DEBUG_PRINT("enter");

	ret = frec_convert_pattern_to_wcs(regex, n, &wregex, &wlen);
	if (ret != REG_OK) {
		DEBUG_PRINTF("returning %d", ret);
		return (ret);
	} else
		ret = frec_compile(preg, wregex, wlen, regex, n, cflags);
	free(wregex);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

int
frec_regcomp(frec_t *preg, const char *regex, int cflags)
{
	int ret;

	DEBUG_PRINT("enter");

	if ((cflags & REG_PEND) && (preg->re_endp >= regex))
		ret = frec_regncomp(preg, regex, preg->re_endp - regex, cflags);
	else
		ret = frec_regncomp(preg, regex, regex ? strlen(regex) : 0, cflags);

	DEBUG_PRINTF("returning %d", ret);
	return ret;
}


int
frec_regwncomp(frec_t *preg, const wchar_t *regex, size_t n, int cflags)
{
	char *sregex;
	size_t slen;
	int ret;

	DEBUG_PRINT("enter");

	ret = frec_convert_pattern_to_mbs(regex, n, &sregex, &slen);
	if (ret != REG_OK) {
		DEBUG_PRINTF("returning %d", ret);
		return (ret);
	} else
		ret = frec_compile(preg, regex, n, sregex, slen, cflags);
	free(sregex);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

int
frec_regwcomp(frec_t *preg, const wchar_t *regex, int cflags)
{
	int ret;

	DEBUG_PRINT("enter");

	if ((cflags & REG_PEND) && (preg->re_wendp >= regex))
		ret = frec_regwncomp(preg, regex, preg->re_wendp - regex, cflags);
	else
		ret = frec_regwncomp(preg, regex, regex ? wcslen(regex) : 0, cflags);

	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

void
frec_regfree(frec_t *preg)
{

	DEBUG_PRINT("enter");

	if (preg->shortcut != NULL) {
		frec_free_fast(preg->shortcut);
		free(preg->shortcut);
	}

	if (preg->heur != NULL) {
		frec_free_heur(preg->heur);
		free(preg->heur);
	}

	_dist_regfree(&preg->orig);
}

int
frec_regnexec(const frec_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = (MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;

	if (eflags & REG_STARTEND)
		CALL_WITH_OFFSET(frec_match(preg, &str[offset], slen,
		    type, nmatch, pmatch, eflags));
	else {
		int ret = frec_match(preg, str,
		    len == (unsigned)-1 ? strlen(str) : len,
		    type, nmatch, pmatch, eflags);
		DEBUG_PRINTF("returning %d", ret);
		return ret;
	}
}
int
frec_regexec(const frec_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{

	int ret = frec_regnexec(preg, str, (unsigned)-1, nmatch,
	    pmatch, eflags);

	DEBUG_PRINTF("returning %d", ret);
	return ret;
}


int
frec_regwnexec(const frec_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = STR_WIDE;

	if (eflags & REG_STARTEND)
		CALL_WITH_OFFSET(frec_match(preg, &str[offset], slen,
		    type, nmatch, pmatch, eflags));
	else {
		int ret = frec_match(preg, str,
		    len == (unsigned)-1 ? wcslen(str) : len,
		    STR_WIDE, nmatch, pmatch, eflags);
		DEBUG_PRINTF("returning %d", ret);
		return ret;
	}
}

int
frec_regwexec(const frec_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret = frec_regwnexec(preg, str, (unsigned)-1, nmatch,
	    pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

int
frec_mregncomp(mregex_t *preg, size_t nr, const char **regex,
    size_t *n, int cflags)
{
	int ret = REG_NOMATCH;
	const wchar_t **wr;
	wchar_t **wregex = NULL;
	size_t *wlen = NULL;
	size_t i = 0;

	DEBUG_PRINT("enter");

	wregex = malloc(nr * sizeof(wchar_t *));
	if (!wregex) {
		DEBUG_PRINT("returning REG_ESPACE");
		return (REG_ESPACE);
	}
	wlen = malloc(nr * sizeof(size_t));
	if (!wlen)
		goto err;

	for (i = 0; i < nr; i++) {
		ret = frec_convert_pattern_to_wcs(regex[i], n[i], &wregex[i], &wlen[i]);
		if (ret != REG_OK)
			goto err;
	}

	wr = (const wchar_t **)wregex;
	ret = frec_mcompile(preg, nr, wr, wlen, regex, n, cflags);

err:
	if (wregex) {
		for (size_t j = 0; j < i; j++)
			if (wregex[j])
				free(wregex[j]);
	}
	if (wlen)
		free(wlen);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

int
frec_mregcomp(mregex_t *preg, size_t nr, const char **regex, int cflags)
{
	int ret;
	size_t *len;

	DEBUG_PRINT("enter");

	len = malloc(nr * sizeof(size_t));
	if (!len) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}

	for (size_t i = 0; i < nr; i++)
		len[i] = strlen(regex[i]);

	ret = frec_mregncomp(preg, nr, regex, len, cflags);
	free(len);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}


int
frec_mregwncomp(mregex_t *preg, size_t nr, const wchar_t **regex,
    size_t *n, int cflags)
{
	int ret = REG_NOMATCH;
	const char **sr;
	char **sregex = NULL;
	size_t *slen = NULL;
	size_t i = 0;

	DEBUG_PRINT("enter");

	sregex = malloc(nr * sizeof(char *));
	if (!sregex) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}
	slen = malloc(nr * sizeof(size_t));
	if (!slen)
		goto err;

	for (i = 0; i < nr; i++) {
		ret = frec_convert_pattern_to_mbs(regex[i], n[i], &sregex[i], &slen[i]);
		if (ret != REG_OK)
			goto err;
	}

	sr = (const char **)sregex;
	ret = frec_mcompile(preg, nr, regex, n, sr, slen, cflags);

err:
	if (sregex) {
		for (size_t j = 0; j < i; j++)
			if (sregex[j])
				free(sregex[j]);
	}
	if (slen)
		free(slen);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

int
frec_mregwcomp(mregex_t *preg, size_t nr, const wchar_t **regex,
    int cflags)
{
	int ret;
	size_t *len;

	DEBUG_PRINT("enter");

	len = malloc(nr * sizeof(size_t));
	if (!len) {
		DEBUG_PRINT("returning REG_ESPACE");
		return REG_ESPACE;
	}

	for (size_t i = 0; i < nr; i++)
		len[i] = wcslen(regex[i]);

	ret = frec_mregwncomp(preg, nr, regex, len, cflags);
	free(len);
	DEBUG_PRINTF("returning %d", ret);
	return (ret);
}

void
frec_mregfree(mregex_t *preg)
{

	DEBUG_PRINT("enter");

	if (!preg) {
		frec_wmfree(preg->searchdata);
		free(preg);
	}
}

int
frec_mregnexec(const mregex_t *preg, const char *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = (MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;

	if (eflags & REG_STARTEND)
		CALL_WITH_OFFSET(frec_mmatch(&str[offset], slen, type,
		    nmatch, pmatch, eflags, preg));
	else {
		int ret = frec_mmatch(str, len == (unsigned)-1 ?
		    strlen(str) : len, type, nmatch, pmatch, eflags, preg);
		DEBUG_PRINTF("returning %d", ret);
		return ret;
	}
}

int
frec_mregexec(const mregex_t *preg, const char *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int ret = frec_mregnexec(preg, str, (unsigned)-1, nmatch,
	    pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}


int
frec_mregwnexec(const mregex_t *preg, const wchar_t *str, size_t len,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{
	int type = STR_WIDE;

	if (eflags & REG_STARTEND)
		CALL_WITH_OFFSET(frec_mmatch(&str[offset], slen, type, nmatch,
		    pmatch, eflags, preg));
	else {
		int ret = frec_mmatch(str, len == (unsigned)-1 ? wcslen(str) : len,
		    STR_WIDE, nmatch, pmatch, eflags, preg);
		DEBUG_PRINTF("returning %d", ret);
		return ret;
	}
}

int
frec_mregwexec(const mregex_t *preg, const wchar_t *str,
    size_t nmatch, frec_match_t pmatch[], int eflags)
{

	int ret = frec_mregwnexec(preg, str, (unsigned)-1, nmatch, pmatch, eflags);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}

size_t
frec_mregerror(int errcode, const mregex_t *preg, int *errpatn, char *errbuf,
    size_t errbuf_size)
{

	DEBUG_PRINT("enter");

	if (errpatn)
		*errpatn = preg->err;

	int ret = _dist_regerror(errcode, NULL, errbuf, errbuf_size);
	DEBUG_PRINTF("returning %d", ret);
	return ret;
}