/* $Id: otherconv.c,v 1.29 2008/05/06 00:11:52 rikster5 Exp $ */

/*******************************************************************************
 * otherconv.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2004 by Amit Aronovitch 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ******************************************************************************/

/*
 * General charset conversion library (using gconv)
 * Convert Palm  <-> Unix:
 * Palm : Any - according to the "other-pda-charset" setup option.
 * Unix : UTF-8
 */

#include "config.h"
#include <string.h>

#ifdef ENABLE_GTK2
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
/* To speed up compilation use these instead of glib.h
#include <glib/gmacros.h>
#include <glib/gconvert.h>
*/
#include "prefs.h"
#include "otherconv.h"
#include "log.h"

static GIConv glob_topda = NULL;
static GIConv glob_frompda = NULL;

#define HOST_CS "UTF-8"

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif


/*
 * strnlen is not ANSI.
 * To avoid messing with conflicting declarations, I just implement my own version.
 * (this is easy & portable might not be very efficient) -- Amit Aronovitch
 */
size_t oc_strnlen(const char *s, size_t maxlen) {
  const char *p,*endp;

  endp = s+maxlen;
  for (p=s;p<endp;++p) if (! *p) break;
  return p-s;
}

void oc_free_iconv(const char *funcname, GIConv conv, char *convname) {
  if (conv != NULL) {
    if (g_iconv_close(conv) != 0) {
      jp_logf(JP_LOG_WARN, "%s: error exit from g_iconv_close(%s)\n",
	      funcname,convname);
    }
  }
}

#define OC_FREE_ICONV(conv) oc_free_iconv(__FUNCTION__, conv,#conv)

/*
 * Convert char_set integer code to iconv charset text string
 */
char *char_set_to_text(int char_set)
{
   switch (char_set)
   {
      case CHAR_SET_1250_UTF:
	 return "CP1250";

      case CHAR_SET_1253_UTF:
	 return "CP1253";

      case CHAR_SET_ISO8859_2_UTF:
	 return "ISO8859-2";

      case CHAR_SET_KOI8_R_UTF:
	 return "KOI8-R";

      case CHAR_SET_1251_UTF:
	 return "CP1251";

      case CHAR_SET_GBK_UTF:
	 return "GBK";

      case CHAR_SET_BIG5_UTF:
	 return "BIG-5";

      case CHAR_SET_SJIS_UTF:
	 return "SJIS";

      case CHAR_SET_1255_UTF:
	 return "CP1255";

      case CHAR_SET_949_UTF:
	 return "CP949";

      case CHAR_SET_1252_UTF:
      default:
	 return "CP1252";
   }
}

/*
 * Module initialization function
 *  Call this before any conversion routine.
 *  Can also be used if you want to reread the 'charset' option
 *
 * Returns 0 if OK, -1 if iconv could not be initialized
 *  (probably because of bad charset string)
 */
int otherconv_init() {
  long char_set;

  get_pref(PREF_CHAR_SET, &char_set, NULL);

  /* (re)open the "to" iconv */
  OC_FREE_ICONV(glob_topda);
  glob_topda = g_iconv_open(char_set_to_text(char_set), HOST_CS);
  if (glob_topda == (GIConv)(-1))
     return EXIT_FAILURE;

  /* (re)open the "from" iconv */
  OC_FREE_ICONV(glob_frompda);
  glob_frompda = g_iconv_open(HOST_CS, char_set_to_text(char_set));
  if (glob_frompda == (GIConv)(-1)) {
    OC_FREE_ICONV(glob_topda);
     return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/*
 * Module finalization function
 * closes the iconvs
 */
void otherconv_free() {
  OC_FREE_ICONV(glob_topda);
  OC_FREE_ICONV(glob_frompda);
}

/*
 *           Conversion to UTF using g_convert_with_iconv
 *     A new buffer is now allocated and the old one remains unchanged
 */
char *other_to_UTF(const char *buf, int buf_len)
{
  size_t rc;
  char *outbuf;
  gsize bytes_read;
  GError *err = NULL;

#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
#endif
  rc = g_iconv(glob_frompda, NULL, NULL, NULL, NULL);
#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__, buf);
#endif

  outbuf = (char *)g_convert_with_iconv((gchar *)buf,
      oc_strnlen(buf, buf_len) +1, /* see Debian bug #309082 for the +1 */
      glob_frompda, &bytes_read, NULL, &err);
  if (err != NULL || bytes_read < oc_strnlen (buf, buf_len)) {
      char c;
      char *head, *tail;
      int outbuf_len;
      char *tmp_buf = (char *)buf;
      static int call_depth = 0;

      if (0 == call_depth)
	 jp_logf(JP_LOG_WARN, "%s:%s g_convert_with_iconv error: %s, buff: %s\n",
	    __FILE__, __FUNCTION__, err ? err->message : "last char truncated",
	 buf);
      if (err != NULL)
	 g_error_free(err);
      else
	 g_free(outbuf);

      /* convert the head, skip the problematic char, convert the tail */
      c = tmp_buf[bytes_read];
      tmp_buf[bytes_read] = '\0';
      head = g_convert_with_iconv(tmp_buf, 
                                  oc_strnlen(tmp_buf, buf_len),
                                  glob_frompda, 
                                  &bytes_read, 
                                  NULL, NULL);
      tmp_buf[bytes_read] = c;

      call_depth++;
      tail = other_to_UTF(tmp_buf + bytes_read +1, buf_len - bytes_read - 1);
      call_depth--;

      outbuf_len = strlen(head) +4 + strlen(tail)+1;
      outbuf = g_malloc(outbuf_len);
      g_snprintf(outbuf, outbuf_len, "%s\\%02X%s", head, (unsigned char)c, tail);

      g_free(head);
      g_free(tail);
  }

#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__, outbuf);
#endif
  /*
   * Note: outbuf was allocated by glib, so should be freed with g_free
   * To be 100% safe, I should have done strncpy to a new malloc-allocated string.
   * (at least under an 'if (!g_mem_is_system_malloc())' test)
   *
   * However, unless you replace the default GMemVTable, freeing with C free should be fine
   *  so I decided this is not worth the overhead  -- Amit Aronovitch
   */
  return outbuf;
}

/*
 *           Conversion to pda encoding using g_iconv
 */
void UTF_to_other(char *const buf, int buf_len)
{
  gsize inleft,outleft;
  gchar *inptr, *outptr;
  size_t rc;
  char *errstr;
  char buf_out[1000], *buf_out_ptr = NULL;

#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
#endif
  rc = g_iconv(glob_topda, NULL, NULL, NULL, NULL);
#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__, buf);
#endif

  inleft = oc_strnlen(buf,buf_len);
  outleft = buf_len-1;
  inptr = buf;

   if (buf_len > sizeof(buf_out))
   {
      buf_out_ptr = malloc(buf_len);
      if (NULL == buf_out_ptr)
      {
	 jp_logf(JP_LOG_WARN, "no more memory");
	 return;
      }
      outptr = buf_out_ptr;
   }
   else
      outptr = buf_out;

  rc = g_iconv(glob_topda, &inptr, &inleft, &outptr, &outleft);
  *outptr = 0;
  if ((size_t)(-1) == rc) {
    switch (errno) {
    case EILSEQ:
      errstr = "iconv: unconvertable sequence at place %d\n";
      break;
    case EINVAL:
      errstr = "iconv: incomplete UTF-8 sequence at place %d\n";
      break;
    case E2BIG:
      errstr = "iconv: buffer filled. stopped at place %d\n";
      break;
    default:
      errstr = "iconv: unexpected error at place %d\n";
    }
    jp_logf(JP_LOG_WARN, errstr, inptr - buf);
  }

   if (buf_out_ptr)
   {
      g_strlcpy(buf, buf_out_ptr, buf_len);
      free(buf_out_ptr);
   }
   else
      g_strlcpy(buf, buf_out, buf_len);

#ifdef OTHERCONV_DEBUG
  jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__, buf);
#endif
}

#else

char *other_to_UTF(const char *buf, int buf_len)
{
	return strdup(buf);
}

void UTF_to_other(char *const buf, int buf_len)
{
}

#endif

