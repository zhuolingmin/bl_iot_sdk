/*
 * Copyright (c) 2020 Bouffalolab.
 *
 * This file is part of
 *     *** Bouffalolab Software Dev Kit ***
 *      (see www.bouffalolab.com).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of Bouffalo Lab nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

//#define CHAR_BIT	8
//FIXME no ugly declare
extern int bl_uart_data_send(uint8_t id, uint8_t data);

enum flag {
	FL_ZERO		= 0x01,	/* Zero modifier */
	FL_MINUS	= 0x02,	/* Minus modifier */
	FL_PLUS		= 0x04,	/* Plus modifier */
	FL_TICK		= 0x08,	/* ' modifier */
	FL_SPACE	= 0x10,	/* Space modifier */
	FL_HASH		= 0x20,	/* # modifier */
	FL_SIGNED	= 0x40,	/* Number is signed */
	FL_UPPER	= 0x80	/* Upper case digits */
};

/* These may have to be adjusted on certain implementations */
enum ranks {
	rank_char	= -2,
	rank_short	= -1,
	rank_int	=  0,
	rank_long	=  1,
	rank_longlong	=  2
};

#define MIN_RANK	rank_char
#define MAX_RANK	rank_longlong

#define INTMAX_RANK	rank_longlong
#define SIZE_T_RANK	rank_long
#define PTRDIFF_T_RANK	rank_long

#define EMIT(x) { if (o < n) { *q++ = (x); } o++; }

static size_t
format_int(char *q, size_t n, uintmax_t val, unsigned int flags,
	   int base, int width, int prec)
{
	char *qq;
	size_t o = 0, oo;
	static const char lcdigits[] = "0123456789abcdef";
	static const char ucdigits[] = "0123456789ABCDEF";
	const char *digits;
	uintmax_t tmpval;
	int minus = 0;
	int ndigits = 0, nchars;
	int tickskip, b4tick;

	/* Select type of digits */
	digits = (flags & FL_UPPER) ? ucdigits : lcdigits;

	/* If signed, separate out the minus */
	if (flags & FL_SIGNED && (intmax_t) val < 0) {
		minus = 1;
		val = (uintmax_t) (-(intmax_t) val);
	}

	/* Count the number of digits needed.  This returns zero for 0. */
	tmpval = val;
	while (tmpval) {
		tmpval /= base;
		ndigits++;
	}

	/* Adjust ndigits for size of output */

	if (flags & FL_HASH && base == 8) {
		if (prec < ndigits + 1)
			prec = ndigits + 1;
	}

	if (ndigits < prec) {
		ndigits = prec;	/* Mandatory number padding */
	} else if (val == 0) {
		ndigits = 1;	/* Zero still requires space */
	}

	/* For ', figure out what the skip should be */
	if (flags & FL_TICK) {
		tickskip = (base == 16) ? 4 : 3;
	} else {
		tickskip = ndigits;	/* No tick marks */
	}

	/* Tick marks aren't digits, but generated by the number converter */
	ndigits += (ndigits - 1) / tickskip;

	/* Now compute the number of nondigits */
	nchars = ndigits;

	if (minus || (flags & (FL_PLUS | FL_SPACE)))
		nchars++;	/* Need space for sign */
	if ((flags & FL_HASH) && base == 16) {
		nchars += 2;	/* Add 0x for hex */
	}

	/* Emit early space padding */
	if (!(flags & (FL_MINUS | FL_ZERO)) && width > nchars) {
		while (width > nchars) {
			EMIT(' ');
			width--;
		}
	}

	/* Emit nondigits */
	if (minus) {
		EMIT('-');
	} else if (flags & FL_PLUS) {
		EMIT('+');
	} else if (flags & FL_SPACE) {
		EMIT(' ');
	}

	if ((flags & FL_HASH) && base == 16) {
		EMIT('0');
		EMIT((flags & FL_UPPER) ? 'X' : 'x');
	}

	/* Emit zero padding */
	if ((flags & (FL_MINUS | FL_ZERO)) == FL_ZERO && width > ndigits) {
		while (width > nchars) {
			EMIT('0');
			width--;
		}
	}

	/* Generate the number.  This is done from right to left. */
	q += ndigits;		/* Advance the pointer to end of number */
	o += ndigits;
	qq = q;
	oo = o;			/* Temporary values */

	b4tick = tickskip;
	while (ndigits > 0) {
		if (!b4tick--) {
			qq--;
			oo--;
			ndigits--;
			if (oo < n)
				*qq = '_';
			b4tick = tickskip - 1;
		}
		qq--;
		oo--;
		ndigits--;
		if (oo < n)
			*qq = digits[val % base];
		val /= base;
	}

	/* Emit late space padding */
	while ((flags & FL_MINUS) && width > nchars) {
		EMIT(' ');
		width--;
	}

	return o;
}

#define ZEROPAD  	(1<<0)	/* Pad with zero */
#define SIGN    	(1<<1)	/* Unsigned/signed long */
#define PLUS    	(1<<2)	/* Show plus */
#define SPACE   	(1<<3)	/* Spacer */
#define LEFT    	(1<<4)	/* Left justified */
#define HEX_PREP 	(1<<5)	/* 0x */
#define UPPERCASE   (1<<6)	/* 'ABCDEF' */

#include <math.h>
#define CVTBUFSIZE 80

static char *cvt(double arg, int ndigits, int *decpt, int *sign, char *buf, int eflag)
{
  int r2;
  double fi, fj;
  char *p, *p1;

  if (ndigits < 0) ndigits = 0;
  if (ndigits >= CVTBUFSIZE - 1) ndigits = CVTBUFSIZE - 2;
  r2 = 0;
  *sign = 0;
  p = &buf[0];
  if (arg < 0)
  {
    *sign = 1;
    arg = -arg;
  }
  arg = modf(arg, &fi);
  p1 = &buf[CVTBUFSIZE];

  if (fi != 0) 
  {
    p1 = &buf[CVTBUFSIZE];
    while (fi != 0) 
    {
      fj = modf(fi / 10, &fi);
      *--p1 = (int)((fj + 0.03) * 10) + '0';
      r2++;
    }
    while (p1 < &buf[CVTBUFSIZE]) *p++ = *p1++;
  }
  else if (arg > 0)
  {
    while ((fj = arg * 10) < 1) 
    {
      arg = fj;
      r2--;
    }
  }
  p1 = &buf[ndigits];
  if (eflag == 0) p1 += r2;
  *decpt = r2;
  if (p1 < &buf[0]) 
  {
    buf[0] = '\0';
    return buf;
  }
  while (p <= p1 && p < &buf[CVTBUFSIZE])
  {
    arg *= 10;
    arg = modf(arg, &fj);
    *p++ = (int) fj + '0';
  }
  if (p1 >= &buf[CVTBUFSIZE]) 
  {
    buf[CVTBUFSIZE - 1] = '\0';
    return buf;
  }
  p = p1;
  *p1 += 5;
  while (*p1 > '9')
  {
    *p1 = '0';
    if (p1 > buf)
      ++*--p1;
    else
    {
      *p1 = '1';
      (*decpt)++;
      if (eflag == 0)
      {
        if (p > buf) *p = '0';
        p++;
      }
    }
  }
  *p = '\0';
  return buf;
}

char *ecvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf)
{
  return cvt(arg, ndigits, decpt, sign, buf, 1);
}

char *fcvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf)
{
  return cvt(arg, ndigits, decpt, sign, buf, 0);
}

static void ee_bufcpy(char *d, char *s, int count); 
 
void ee_bufcpy(char *pd, char *ps, int count) {
	char *pe=ps+count;
	while (ps!=pe)
		*pd++=*ps++;
}

static void parse_float(double value, char *buffer, char fmt, int precision)
{
  int decpt, sign, exp, pos;
  char *digits = NULL;
  char cvtbuf[80];
  int capexp = 0;
  int magnitude;

  if (fmt == 'G' || fmt == 'E')
  {
    capexp = 1;
    fmt += 'a' - 'A';
  }

  if (fmt == 'g')
  {
    digits = ecvtbuf(value, precision, &decpt, &sign, cvtbuf);
    magnitude = decpt - 1;
    if (magnitude < -4  ||  magnitude > precision - 1)
    {
      fmt = 'e';
      precision -= 1;
    }
    else
    {
      fmt = 'f';
      precision -= decpt;
    }
  }

  if (fmt == 'e')
  {
    digits = ecvtbuf(value, precision + 1, &decpt, &sign, cvtbuf);

    if (sign) *buffer++ = '-';
    *buffer++ = *digits;
    if (precision > 0) *buffer++ = '.';
    ee_bufcpy(buffer, digits + 1, precision);
    buffer += precision;
    *buffer++ = capexp ? 'E' : 'e';

    if (decpt == 0)
    {
      if (value == 0.0)
        exp = 0;
      else
        exp = -1;
    }
    else
      exp = decpt - 1;

    if (exp < 0)
    {
      *buffer++ = '-';
      exp = -exp;
    }
    else
      *buffer++ = '+';

    buffer[2] = (exp % 10) + '0';
    exp = exp / 10;
    buffer[1] = (exp % 10) + '0';
    exp = exp / 10;
    buffer[0] = (exp % 10) + '0';
    buffer += 3;
  }
  else if (fmt == 'f')
  {
    digits = fcvtbuf(value, precision, &decpt, &sign, cvtbuf);
    if (sign) *buffer++ = '-';
    if (*digits)
    {
      if (decpt <= 0)
      {
        *buffer++ = '0';
        *buffer++ = '.';
        for (pos = 0; pos < -decpt; pos++) *buffer++ = '0';
        while (*digits) *buffer++ = *digits++;
      }
      else
      {
        pos = 0;
        while (*digits)
        {
          if (pos++ == decpt) *buffer++ = '.';
          *buffer++ = *digits++;
        }
      }
    }
    else
    {
      *buffer++ = '0';
      if (precision > 0)
      {
        *buffer++ = '.';
        for (pos = 0; pos < precision; pos++) *buffer++ = '0';
      }
    }
  }

  *buffer = '\0';
}

static void decimal_point(char *buffer)
{
  while (*buffer)
  {
    if (*buffer == '.') return;
    if (*buffer == 'e' || *buffer == 'E') break;
    buffer++;
  }

  if (*buffer)
  {
    int n = strnlen(buffer,256);
    while (n > 0) 
    {
      buffer[n + 1] = buffer[n];
      n--;
    }

    *buffer = '.';
  }
  else
  {
    *buffer++ = '.';
    *buffer = '\0';
  }
}

static void cropzeros(char *buffer)
{
  char *stop;

  while (*buffer && *buffer != '.') buffer++;
  if (*buffer++)
  {
    while (*buffer && *buffer != 'e' && *buffer != 'E') buffer++;
    stop = buffer--;
    while (*buffer == '0') buffer--;
    if (*buffer == '.') buffer--;
    while (buffer!=stop)
		*++buffer=0;
  }
}

static char *flt(char *str, double num, int size, int precision, char fmt, int flags)
{
  char tmp[80];
  char c, sign;
  int n, i;

  // Left align means no zero padding
  if (flags & LEFT) flags &= ~ZEROPAD;

  // Determine padding and sign char
  c = (flags & ZEROPAD) ? '0' : ' ';
  sign = 0;
  if (flags & SIGN)
  {
    if (num < 0.0)
    {
      sign = '-';
      num = -num;
      size--;
    }
    else if (flags & PLUS)
    {
      sign = '+';
      size--;
    }
    else if (flags & SPACE)
    {
      sign = ' ';
      size--;
    }
  }

  // Compute the precision value
  if (precision < 0)
    precision = 6; // Default precision: 6

  // Convert floating point number to text
  parse_float(num, tmp, fmt, precision);

  if ((flags & HEX_PREP) && precision == 0) decimal_point(tmp);
  if (fmt == 'g' && !(flags & HEX_PREP)) cropzeros(tmp);

  n = strnlen(tmp,256);

  // Output number with alignment and padding
  size -= n;
  if (!(flags & (ZEROPAD | LEFT))) while (size-- > 0) *str++ = ' ';
  if (sign) *str++ = sign;
  if (!(flags & LEFT)) while (size-- > 0) *str++ = c;
  for (i = 0; i < n; i++) *str++ = tmp[i];
  while (size-- > 0) *str++ = ' ';

  return str;
}


/*use O0 preventing consuming more stack*/
int __attribute__((optimize("O1"))) vsnprintf(char *buffer, size_t n, const char *format, va_list ap)
{
	const char *p = format;
	char ch;
	char *q = buffer;
	size_t o = 0;		/* Number of characters output */
	uintmax_t val = 0;
	int rank = rank_int;	/* Default rank */
	int width = 0;
	int prec = -1;
	int base;
	size_t sz;
	unsigned int flags = 0;
	enum {
		st_normal,	/* Ground state */
		st_flags,	/* Special flags */
		st_width,	/* Field width */
		st_prec,	/* Field precision */
		st_modifiers	/* Length or conversion modifiers */
	} state = st_normal;
	const char *sarg;	/* %s string argument */
	char carg;		/* %c char argument */
	int slen;		/* String length */

	while ((ch = *p++)) {
		switch (state) {
		case st_normal:
			if (ch == '%') {
				state = st_flags;
				flags = 0;
				rank = rank_int;
				width = 0;
				prec = -1;
			} else {
				EMIT(ch);
			}
			break;

		case st_flags:
			switch (ch) {
			case '-':
				flags |= FL_MINUS;
				break;
			case '+':
				flags |= FL_PLUS;
				break;
			case '\'':
				flags |= FL_TICK;
				break;
			case ' ':
				flags |= FL_SPACE;
				break;
			case '#':
				flags |= FL_HASH;
				break;
			case '0':
				flags |= FL_ZERO;
				break;
			default:
				state = st_width;
				p--;	/* Process this character again */
				break;
			}
			break;

		case st_width:
			if (ch >= '0' && ch <= '9') {
				width = width * 10 + (ch - '0');
			} else if (ch == '*') {
				width = va_arg(ap, int);
				if (width < 0) {
					width = -width;
					flags |= FL_MINUS;
				}
			} else if (ch == '.') {
				prec = 0;	/* Precision given */
				state = st_prec;
			} else {
				state = st_modifiers;
				p--;	/* Process this character again */
			}
			break;

		case st_prec:
			if (ch >= '0' && ch <= '9') {
				prec = prec * 10 + (ch - '0');
			} else if (ch == '*') {
				prec = va_arg(ap, int);
				if (prec < 0)
					prec = -1;
			} else {
				state = st_modifiers;
				p--;	/* Process this character again */
			}
			break;

		case st_modifiers:
			switch (ch) {
				/* Length modifiers - nonterminal sequences */
			case 'h':
				rank--;	/* Shorter rank */
				break;
			case 'l':
				rank++;	/* Longer rank */
				break;
			case 'j':
				rank = INTMAX_RANK;
				break;
			case 'z':
				rank = SIZE_T_RANK;
				break;
			case 't':
				rank = PTRDIFF_T_RANK;
				break;
			case 'L':
			case 'q':
				rank += 2;
				break;
			default:
				/* Output modifiers - terminal sequences */

				/* Next state will be normal */
				state = st_normal;

				/* Canonicalize rank */
				if (rank < MIN_RANK)
					rank = MIN_RANK;
				else if (rank > MAX_RANK)
					rank = MAX_RANK;

				switch (ch) {
				case 'P':	/* Upper case pointer */
					flags |= FL_UPPER;
                    __attribute__ ((fallthrough));
					/* fall through */
				case 'p':	/* Pointer */
					base = 16;
					prec = (CHAR_BIT*sizeof(void *)+3)/4;
					flags |= FL_HASH;
					val = (uintmax_t)(uintptr_t)
						va_arg(ap, void *);
					goto is_integer;

				case 'd':	/* Signed decimal output */
				case 'i':
					base = 10;
					flags |= FL_SIGNED;
					switch (rank) {
					case rank_char:
						/* Yes, all these casts are
						   needed... */
						val = (uintmax_t)(intmax_t)
							(signed char)
							va_arg(ap, signed int);
						break;
					case rank_short:
						val = (uintmax_t)(intmax_t)
							(signed short)
							va_arg(ap, signed int);
						break;
					case rank_int:
						val = (uintmax_t)(intmax_t)
						    va_arg(ap, signed int);
						break;
					case rank_long:
						val = (uintmax_t)(intmax_t)
						    va_arg(ap, signed long);
						break;
					case rank_longlong:
						val = (uintmax_t)(intmax_t)
						    va_arg(ap,
							   signed long long);
						break;
					}
					goto is_integer;
				case 'o':	/* Octal */
					base = 8;
					goto is_unsigned;
				case 'u':	/* Unsigned decimal */
					base = 10;
					goto is_unsigned;
				case 'X':	/* Upper case hexadecimal */
					flags |= FL_UPPER;
                    __attribute__ ((fallthrough));
					/* fall through */
				case 'x':	/* Hexadecimal */
					base = 16;
					goto is_unsigned;

				is_unsigned:
					switch (rank) {
					case rank_char:
						val = (uintmax_t)
							(unsigned char)
							va_arg(ap, unsigned
							       int);
						break;
					case rank_short:
						val = (uintmax_t)
							(unsigned short)
							va_arg(ap, unsigned
							       int);
						break;
					case rank_int:
						val = (uintmax_t)
							va_arg(ap, unsigned
							       int);
						break;
					case rank_long:
						val = (uintmax_t)
							va_arg(ap, unsigned
							       long);
						break;
					case rank_longlong:
						val = (uintmax_t)
							va_arg(ap, unsigned
							       long long);
						break;
					}
					/* fall through */

				is_integer:
					sz = format_int(q, (o < n) ? n - o : 0,
							val, flags, base,
							width, prec);
					q += sz;
					o += sz;
					break;

				case 'c':	/* Character */
					carg = (char)va_arg(ap, int);
					sarg = &carg;
					slen = 1;
					goto is_string;
				case 's':	/* String */
					sarg = va_arg(ap, const char *);
					sarg = sarg ? sarg : "(null)";
					slen = strlen(sarg);
					goto is_string;

				is_string:
					{
						char sch;
						int i;

						if (prec != -1 && slen > prec)
							slen = prec;

						if (width > slen
						    && !(flags & FL_MINUS)) {
							char pad =
							    (flags & FL_ZERO) ?
							    '0' : ' ';
							while (width > slen) {
								EMIT(pad);
								width--;
							}
						}
						for (i = slen; i; i--) {
							sch = *sarg++;
							EMIT(sch);
						}
						if (width > slen
						    && (flags & FL_MINUS)) {
							while (width > slen) {
								EMIT(' ');
								width--;
							}
						}
					}
					break;

				case 'n':
					{
						/* Output the number of
						   characters written */

						switch (rank) {
						case rank_char:
							*va_arg(ap,
								signed char *)
								= o;
							break;
						case rank_short:
							*va_arg(ap,
								signed short *)
								= o;
							break;
						case rank_int:
							*va_arg(ap,
								signed int *)
								= o;
							break;
						case rank_long:
							*va_arg(ap,
								signed long *)
								= o;
							break;
						case rank_longlong:
							*va_arg(ap,
								signed long long *)
								= o;
							break;
						}
					}
					break;
#ifndef DISABLE_PRINT_FLOAT
				case 'f':
					{
						q = flt(q, va_arg(ap, double), width, prec, ch, SIGN);
        		continue;
					}
#endif
				default:	/* Anything else, including % */
					EMIT(ch);
					break;
				}
			}
		}
	}

	/* Null-terminate the string */
	if (o < n)
		*q = '\0';	/* No overflow */
	else if (n > 0)
		buffer[n - 1] = '\0';	/* Overflow - terminate at end of buffer */

	return o;
}

#ifdef SYS_BIG_DEBUG_BUFFER
static char string[2048];
#else
static char string[512];
#endif

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, sizeof(string) - 32, format, ap);
}

extern volatile bool sys_log_all_enable;

void vprint(const char *fmt, va_list argp)
{
    char *str;
    int ch;

    if (sys_log_all_enable) {
        str = string;
        if (0 < vsprintf(string, fmt, argp)) {
            while ('\0' != (ch = *(str++))) {
#if !defined(DISABLE_PRINT)
                bl_uart_data_send(0, ch);
#endif
            }
        }
    }
}

int bl_putchar(int c)
{
#if !defined(DISABLE_PRINT)
    bl_uart_data_send(0, c);
#endif
    return 0;
}

int puts(const char *s)
{
    int counter = 0;
    char c;

    if (sys_log_all_enable) {
        while ('\0' != (c = *(s++))) {
#if !defined(DISABLE_PRINT)
            bl_uart_data_send(0, c);
#endif
            counter++;
        }
    }
    return counter;
}

int printf(const char *fmt, ...)
{
    va_list argp;

    if (sys_log_all_enable) {
        va_start(argp, fmt);
        vprint(fmt, argp);
        va_end(argp);
    }

    return 0;
}

int sprintf(char *buffer, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vsnprintf(buffer, ~(size_t) 0, format, ap);
	va_end(ap);

	return rv;
}

int snprintf(char *buffer, size_t n, const char *format, ...)
{
	va_list ap;
	int rv;

	va_start(ap, format);
	rv = vsnprintf(buffer, n, format, ap);
	va_end(ap);
	return rv;
}

void vMainUARTPrintString(char *pcString)
{
    puts(pcString);
}
