
/*
 *
  Copyright (c) Sangoma Technologies, 2018-2024
  Copyright (c) Dialogic(R), 2009-2014.
 *
  This source file is supplied for the use with
  Sangoma (formerly Dialogic) range of DIVA Server Adapters.
 *
  File Revision :    2.1
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#if !defined(__KERNEL_VERSION_GT_2_4__) /* { */
# include <linux/kernel.h>
# include <linux/tqueue.h>
# include <linux/types.h>
# include <linux/timer.h>
# include <linux/mm.h>
# include <linux/major.h>
# include <linux/string.h>
# include "sys_if.h"
# include "divamemi.h"
#endif /* } */

void *sysPageAlloc (dword *pSize, void **pHandle) {
	unsigned long i;
	char* p;

	if (!(*pHandle = diva_mem_malloc (*pSize))) {
		return ((void*)0);
	}

	i = *pSize;
	p = (char*)*pHandle;
	while (i--)
		*p++ = 0;

	return (*pHandle);
}

#if 0
void *sysPageRealloc (dword *pSize, void **pHandle) {

	if (*pHandle) {
		sysPageFree (*pHandle);
		*pHandle = 0;
	}

	*pHandle = (void*)sysPageAlloc(pSize, pHandle);

	return (*pHandle);
}
#endif

void sysPageFree (void *Handle) {
	if (Handle) {
		diva_mem_free (Handle);
	}
	return;
}

void *sysHeapAlloc (dword Size) {
	void *Address = 0;
	int i;
	char* p;

	if (Size) {
		Address = diva_mem_malloc(Size);
	}

	if (!Address) {
		return (0);
	}
	p = Address;
	i = Size;

	while (i--) {
		*p++ = 0;
	}

	return (Address);
}

void sysHeapFree (void *Address) {
	if (Address) {
		diva_mem_free (Address);
	}
}

void *mem_cpy (void *s1, void *s2, unsigned long len)
{

	memcpy((s1),(s2),(len));
	return (s1);

}

int mem_cmp (void *s1, void *s2, unsigned long len)
{
   
	if (memcmp(s2, s1, len) == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
	
	return 1;
}

void *mem_set (void *s1, int c, unsigned long len)
{
	int i;
	unsigned char *item;

	item = (unsigned char *)s1;
	for (i = 0; i < len; i++)
	{
		item[i] = (unsigned char)c;
	}
	return (s1);
}

void *mem_zero (void *s1, unsigned long len)
{

	memset(s1, 0, len);
	return (s1);
}

int mem_i_cmp (void *s1, void *s2, unsigned long len)
{
	unsigned char *cs1, *cs2;
	unsigned char c1, c2;
	for (cs1 = s1, cs2 = s2; len--; ) {
		c1 = *cs1++; c2 = *cs2++;
		if (c1 >= 'A' && c1 <= 'Z') c1 |= ' ';
		if (c2 >= 'A' && c2 <= 'Z') c2 |= ' ';
		if (c1 != c2) return (1);
	}
	return (0);
}

int str_len (char *s1)
{
	return(strlen(s1));
}

char *str_chr (char *s1, int c)
{
	for ( ; ; s1++ ) {
		if (*s1 == c) return (s1);
		if (*s1 == 0) break;
	}
	return (0);
}

char *str_rchr (char *s1, int c)
{
	char *s2 = str_chr (s1, 0);
	for ( ; s2 >= s1 ; s2-- ) {
		if (*s2 == c) return (s2);
	}
	return (0);
}

char *str_cpy (char *s1, const char *s2)
{
	return(strcpy (s1, s2));
}

int str_cmp (char *s1, char *s2)
{
	return (strcmp (s1, s2));
}

int str_i_cmp (char *s1, char *s2)
{
	unsigned char c1, c2;

	
	do {
		c1 = *s1++; c2 = *s2++;
		if (c1 >= 'A' && c1 <= 'Z') c1 |= ' ';
		if (c2 >= 'A' && c2 <= 'Z') c2 |= ' ';
		if (c1 != c2) return (1);
	} while (c2);


	return (0);
}

int str_match (char *String, char *Substring)
{
	unsigned long len;
	if((len = strlen(Substring)))
	{
		for (len = strlen(Substring); *String ; String++ )
		{
			if (!mem_i_cmp (String, Substring, len))
				return (1);
		}
	}
	return (0);
}

int str_2_int (char *s)
{
	char *p; int i;
	for  (p = s, i = 0; *p >= '0' && *p <= '9'; p++) {
		i *= 10; i += *p - '0';
	}
	return ((p == s || *p) ? -1 : i );
}

unsigned long sysTimerTicks(void)
{
	unsigned long j;

	j = jiffies;
	if (HZ == 250)
		return (j << 2);
	return (((j / HZ) * 1000) + (((j % HZ) * 1000) / HZ));
}

unsigned long sysLastUpdatedTimerTick(void)
{
	return sysTimerTicks();
}


unsigned char *int_2_str (
	unsigned long value, unsigned char *string,
	unsigned long radix, int width, char prefix)
{
static  unsigned char xdigits[] = "0123456789abcdef";
	unsigned char r, *p, *q, *next;

	p = q = string;

	if (!value) *p++ = '0';

	while (value) {
		*p++ = xdigits[(value % radix)];
		value /= radix;
	}

	while (p - q < width) *p++ = prefix;

	next = p;
	*p-- = '\0';

	while (q < p) {
		r    = *q;
		*q++ = *p;
		*p-- = r;
	}

	return (next);
}

unsigned long sysGetSec (void) {
	return (HZ);
}

int str_n_cmp (const char* s1, const char* s2, unsigned int n) {

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(const unsigned char *)s1 -
			    *(const unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

/* -------------------------------------------------------------------------
    Generic printf
   ------------------------------------------------------------------------- */
/*
 * defines
 */
typedef unsigned long int diva_u_quad_t;
typedef long int diva_quad_t;

/* max size buffer kprintf needs to print quad_t [size in base 8 + \0] */
#define DIVA_KPRINTF_BUFSIZE	(4*sizeof(diva_quad_t) * 8 / 3 + 2)

/*
 * macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit(c) <= 9)
#define	to_char(n)	((n) + '0')

/*
 * flags used during conversion.
 */
#define	ALT		0x001		/* alternate form */
#define	HEXPREFIX	0x002		/* add 0x or 0X prefix */
#define	LADJUST		0x004		/* left adjustment */
#define	LONGDBL		0x008		/* long double; unimplemented */
#define	LONGINT		0x010		/* long integer */
#define	QUADINT		0x020		/* quad integer */
#define	SHORTINT	0x040		/* short integer */
#define	ZEROPAD		0x080		/* zero (as opposed to blank) pad */
#define FPT		0x100		/* Floating point number */

/*
 * To extend shorts properly, we need both signed and unsigned
 * argument extraction methods.
 */
#define	SARG() \
  (flags&QUADINT ? va_arg(ap, diva_quad_t) : \
      flags&LONGINT ? va_arg(ap, long) : \
      flags&SHORTINT ? (long)(short)va_arg(ap, int) : \
      (long)va_arg(ap, int))
#define	UARG() \
  (flags&QUADINT ? va_arg(ap, diva_u_quad_t) : \
      flags&LONGINT ? va_arg(ap, u_long) : \
      flags&SHORTINT ? (u_long)(u_short)va_arg(ap, int) : \
      (u_long)va_arg(ap, u_int))

#define KPRINTF_PUTCHAR(C) do{written++;\
                             if (written >= out_limit) {\
                               *out_overflow = 1;\
                               return (written-1);\
                             }\
                             *sbuf++ = (C);\
                           }while(0)

/*
 * Guts of kernel printf.  Note, we already expect to be in a mutex!
 */
static int kprintf (dword out_limit,
                    int far* out_overflow,
                    const char far *fmt0,
                    char far *sbuf,
                    va_list ap)
{
	dword written = 0;
	char far *fmt;		/* format string */
	int ch;			/* character from fmt */
	int n;			/* handy integer (short term usage) */
	char far *cp;		/* handy char pointer (short term usage) */
	int flags;		/* flags as above */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format (%.3d), or -1 */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */
	diva_u_quad_t _uquad;

	enum { OCT, DEC, HEX } base;/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec */
	int size;		/* size of converted field or string */
	char far *xdigs;		/* digits for [xX] conversion */
	char buf[DIVA_KPRINTF_BUFSIZE]; /* space for %c, %[diouxX] */

	cp = 0;	/* XXX: shutup gcc */
	size = 0;	/* XXX: shutup gcc */

	fmt = (char far *)fmt0;
	ret = 0;

	xdigs = 0;		/* XXX: shut up gcc warning */

	*out_overflow = 0;

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		while (*fmt != '%' && *fmt) {
			KPRINTF_PUTCHAR(*fmt++);
			ret++;
		}
		if (*fmt == 0)
			goto done;

		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';

rflag:		ch = *fmt++;
reswitch:	switch (ch) {

		case ' ':
			/*
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '#':
			flags |= ALT;
			goto rflag;
		case '*':
			/*
			 * ``A negative field width argument is taken as a
			 * - flag followed by a positive field width.''
			 *	-- ANSI X3J11
			 * They don't exclude field widths read from args.
			 */
			if ((width = va_arg(ap, int)) >= 0)
				goto rflag;
			width = -width;
			/* FALLTHROUGH */
		case '-':
			flags |= LADJUST;
			goto rflag;
		case '+':
			sign = '+';
			goto rflag;
		case '.':
			if ((ch = *fmt++) == '*') {
				n = va_arg(ap, int);
				prec = n < 0 ? -1 : n;
				goto rflag;
			}
			n = 0;
			while (is_digit(ch)) {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			}
			prec = n < 0 ? -1 : n;
			goto reswitch;
		case '0':
			/*
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit(ch);
				ch = *fmt++;
			} while (is_digit(ch));
			width = n;
			goto reswitch;
		case 'h':
			flags |= SHORTINT;
			goto rflag;
		case 'l':
			if (*fmt == 'l') {
				fmt++;
				flags |= QUADINT;
			} else {
				flags |= LONGINT;
			}
			goto rflag;
		case 'q':
			flags |= QUADINT;
			goto rflag;
		case 'c':
			*(cp = buf) = (char)va_arg(ap, int);
			size = 1;
			sign = '\0';
			break;
		case 'D':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'd':
		case 'i':
			_uquad = SARG();
			if ((diva_quad_t)_uquad < 0) {
				_uquad = (diva_u_quad_t)(-((diva_quad_t)_uquad));
				sign = '-';
			}
			base = DEC;
			goto number;
		case 'O':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'o':
			_uquad = UARG();
			base = OCT;
			goto nosign;
		case 'p':
			/*
			 * ``The argument shall be a pointer to void.  The
			 * value of the pointer is converted to a sequence
			 * of printable characters, in an implementation-
			 * defined manner.''
			 *	-- ANSI X3J11
			 */
			/* NOSTRICT */
			_uquad = (u_long)va_arg(ap, void *);
			base = HEX;
			xdigs = "0123456789abcdef";
			flags |= HEXPREFIX;
			ch = 'x';
			goto nosign;
		case 's':
			if ((cp = va_arg(ap, char *)) == 0)
				cp = "(null)";
				size = str_len(cp);
			sign = '\0';
			break;
		case 'U':
			flags |= LONGINT;
			/*FALLTHROUGH*/
		case 'u':
			_uquad = UARG();
			base = DEC;
			goto nosign;
		case 'X':
			xdigs = "0123456789ABCDEF";
			goto hex;
		case 'x':
			xdigs = "0123456789abcdef";
hex:			_uquad = UARG();
			base = HEX;
			/* leading 0x/X only if non-zero */
			if (flags & ALT && _uquad != 0)
				flags |= HEXPREFIX;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 */
			cp = buf + DIVA_KPRINTF_BUFSIZE;
			if (_uquad != 0 || prec != 0) {
				/*
				 * Unsigned mod is hard, and unsigned mod
				 * by a constant is easier than that by
				 * a variable; hence this switch.
				 */
				switch (base) {
				case OCT:
					do {
						*--cp = (char)to_char(_uquad & 7);
						_uquad >>= 3;
					} while (_uquad);
					/* handle octal leading 0 */
					if (flags & ALT && *cp != '0')
						*--cp = '0';
					break;

				case DEC:
					/* many numbers are 1 digit */
					while (_uquad >= 10) {
						*--cp = (char)to_char(_uquad % 10);
						_uquad /= 10;
					}
					*--cp = (char)to_char(_uquad);
					break;

				case HEX:
					do {
						*--cp = xdigs[_uquad & 15];
						_uquad >>= 4;
					} while (_uquad);
					break;

				default:
					cp = "A: printf";
					size = str_len(cp);
					goto skipsize;
				}
			}
			size = buf + DIVA_KPRINTF_BUFSIZE - cp;
		skipsize:
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = (char)ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		else if (flags & HEXPREFIX)
			realsz+= 2;

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

		/* prefix */
		if (sign) {
			KPRINTF_PUTCHAR(sign);
		} else if (flags & HEXPREFIX) {
			KPRINTF_PUTCHAR('0');
			KPRINTF_PUTCHAR((char)ch);
		}

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR('0');
		}

		/* leading zeroes from decimal precision */
		n = dprec - size;
		while (n-- > 0)
			KPRINTF_PUTCHAR('0');

		/* the string or number proper */
		while (size--)
			KPRINTF_PUTCHAR(*cp++);
		/* left-adjusting padding (always blank) */
		if (flags & LADJUST) {
			n = width - realsz;
			while (n-- > 0)
				KPRINTF_PUTCHAR(' ');
		}

		/* finally, adjust ret */
		ret += width > realsz ? width : realsz;

	}
done:
	return (ret);
	/* NOTREACHED */
}

int diva_snprintf (char* dst, int length, const char* format, ...) {
	if ((length > 1) && dst && format) {
		int ovl, written;
		va_list ap;

		va_start(ap, format);
		written = kprintf (length, &ovl, format, dst, ap);
		va_end(ap);

		dst[written] = 0;

		return (written);
	} else {
		if (dst) {
			*dst = 0;
		}
		return (0);
	}
}


