/****************************************************************/
/*                                                              */
/*                           portab.h                           */
/*                                                              */
/*                 DOS-C portability typedefs, etc.             */
/*                                                              */
/*                         May 1, 1995                          */
/*                                                              */
/*                      Copyright (c) 1995                      */
/*                      Pasquale J. Villani                     */
/*                      All Rights Reserved                     */
/*                                                              */
/* This file is part of DOS-C.                                  */
/*                                                              */
/* DOS-C is free software; you can redistribute it and/or       */
/* modify it under the terms of the GNU General Public License  */
/* as published by the Free Software Foundation; either version */
/* 2, or (at your option) any later version.                    */
/*                                                              */
/* DOS-C is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of   */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See    */
/* the GNU General Public License for more details.             */
/*                                                              */
/* You should have received a copy of the GNU General Public    */
/* License along with DOS-C; see the file COPYING.  If not,     */
/* write to the Free Software Foundation, 675 Mass Ave,         */
/* Cambridge, MA 02139, USA.                                    */
/****************************************************************/

#include <limits.h>

/****************************************************************/
/*                                                              */
/* Machine dependant portable types. Note that this section is  */
/* used primarily for segmented architectures. Common types and */
/* types used relating to segmented operations are found here.  */
/*                                                              */
/* Be aware that segmented architectures impose on linear       */
/* architectures because they require special types to be used  */
/* throught the code that must be reduced to empty preprocessor */
/* replacements in the linear machine.                          */
/*                                                              */
/* #ifdef <segmeted machine>                                    */
/* # define FAR far                                             */
/* # define NEAR near                                           */
/* #endif                                                       */
/*                                                              */
/* #ifdef <linear machine>                                      */
/* # define FAR                                                 */
/* # define NEAR                                                */
/* #endif                                                       */
/*                                                              */
/****************************************************************/

                                                        /* commandline overflow - removing -DI86 TE */
#if defined(__TURBOC__)

#define I86
#define CDECL   cdecl
#if __TURBOC__ > 0x202
#if __TURBOC__ < 0x400 /* targeted to TC++ 1.0 which is 0x297 (3.1 is 0x410) */
#pragma warn -pia /* possibly incorrect assignment */
#pragma warn -sus /* suspicious pointer conversion */
/*
 * NOTE: The above enable TC++ to build the kernel, but it's not recommended
 * for development. Use [Open]Watcom (the best!) or newer Borland compilers!
 */
#endif
/* printf callers do the right thing for tc++ 1.01 but not tc 2.01 */
#define VA_CDECL
#else
#define VA_CDECL cdecl
#endif
#define PASCAL  pascal
void __int__(int);
#ifndef FORSYS
void __emit__(char, ...);
#define disable() __emit__(0xfa)  /* cli; disable interrupts   */
#define enable()  __emit__(0xfb)  /* sti; enable interrupts    */
#define halt()    __emit__(0xf4)  /* hlt; halt until interrupt */
#endif

#elif defined	(_MSC_VER)

#define I86
#define asm __asm
#pragma warning(disable: 4761) /* "integral size mismatch in argument;

                                   conversion supplied" */
#ifndef CDECL
#define CDECL   _cdecl
#define VA_CDECL
#define PASCAL  pascal
#endif
#define __int__(intno) asm int intno;
#define disable() asm cli
#define enable() asm sti
#define halt() asm hlt
#define _CS getCS()
static unsigned short __inline getCS(void)
{
  asm mov ax, cs;
}
#define _SS getSS()
static unsigned short __inline getSS(void)
{
  asm mov ax, ss;
}

#elif defined(__WATCOMC__)      /* don't know a better way */

#ifdef __DOS__
#define I86
#define __int__(intno) asm int intno;
void disable(void);
#pragma aux disable = "cli" modify exact [];
void enable(void);
#pragma aux enable = "sti" modify exact [];
void halt(void);
#pragma aux halt = "hlt" modify exact [];
#define asm __asm
#define far __far
#define CDECL   __cdecl
#define VA_CDECL
#define PASCAL  pascal
#define _CS getCS()
unsigned short getCS(void);
#pragma aux getCS = "mov dx,cs" value [dx] modify exact[dx];
#define _SS getSS()
unsigned short getSS(void);
#pragma aux getSS = "mov dx,ss" value [dx] modify exact[dx];

#if !defined(FORSYS) && !defined(EXEFLAT) && _M_IX86 >= 300
#pragma aux default parm [ax dx cx] modify [ax dx es fs] /* min.unpacked size */
/* #pragma aux default parm [ax dx] modify [ax bx cx dx es fs]min.packed size */
#endif
#endif /* DOS */

/* enable Possible loss of precision warning for compatibility with Borland */
#pragma enable_message(130)

#if _M_IX86 >= 300 || defined(M_I386)
#define I386
#endif

#elif defined (_MYMC68K_COMILER_)

#define MC68K

#elif defined(__GNUC__)
/* for warnings only ! */
#define MC68K

#else
#error Unknown compiler
We might even deal with a pre-ANSI compiler. This will certainly not compile.
#endif

#ifdef I86
#if _M_IX86 >= 300 || defined(M_I386)
#define I386
#elif _M_IX86 >= 100 || defined(M_I286)
#define I186
#endif
#endif

#ifdef MC68K
#define far                     /* No far type          */
#define interrupt               /* No interrupt type    */
#define VOID           void
#define FAR                     /* linear architecture  */
#define NEAR                    /*    "        "        */
#define INRPT          interrupt
#define REG            register
#define API            int      /* linear architecture  */
#define NONNATIVE
#define PARASIZE       4096     /* "paragraph" size     */
#define CDECL
#define PASCAL
#ifdef __GNUC__
#define CONST          const
#define PROTO
typedef __SIZE_TYPE__  size_t;
#else
#define CONST
typedef unsigned       size_t;
#endif
#endif
#ifdef I86
#define VOID           void
#define FAR            far      /* segment architecture */
#define NEAR           near     /*    "          "      */
#define INRPT          interrupt
#define CONST          const
#define REG            register
#define API            int far pascal   /* segment architecture */
#define NATIVE
#define PARASIZE       16       /* "paragraph" size     */
typedef unsigned       size_t;
#endif
           /* functions, that are shared between C and ASM _must_ 
              have a certain calling standard. These are declared
              as 'ASMCFUNC', and is (and will be ?-) cdecl */
#define ASMCFUNC CDECL
#define ASMPASCAL PASCAL
#define ASM ASMCFUNC
/*                                                              */
/* Boolean type & definitions of TRUE and FALSE boolean values  */
/*                                                              */
typedef int BOOL;
#ifndef FALSE
#define FALSE           (1==0)
#define TRUE            (1==1)
#endif

/*                                                              */
/* Common pointer types                                         */
/*                                                              */
#ifndef NULL
#define NULL            0
#endif

/*                                                              */
/* Convienence defines                                          */
/*                                                              */
#define FOREVER         while(TRUE)
#ifndef max
#define max(a,b)       (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b)       (((a) < (b)) ? (a) : (b))
#endif

/*                                                              */
/* Common byte, 16 bit and 32 bit types                         */
/*                                                              */
#ifndef _WIN32
typedef char BYTE;
typedef short WORD;
typedef long DWORD;
#endif

typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef unsigned long UDWORD;

typedef short SHORT;

typedef unsigned int BITS;      /* for use in bit fields(!)     */

typedef int COUNT;
typedef unsigned int UCOUNT;
typedef unsigned long ULONG;

#ifdef WITHFAT32
typedef unsigned long CLUSTER;
#else
typedef unsigned short CLUSTER;
#endif
typedef unsigned short UNICODE;

#if defined(STATICS) || defined(__WATCOMC__)
#define STATIC static		 /* local calls inside module */
#else
#define STATIC
#endif

#ifdef UNIX
typedef char FAR *ADDRESS;
#elif _WIN32
typedef void * ADDRESS;
#else
typedef void FAR *ADDRESS;
#endif

#ifdef STRICT
typedef signed long LONG;
#else
#define LONG long
#endif

typedef UWORD ofs_t;
typedef UWORD seg_t;

#define lonibble(v) (0x0f & (v))
#define hinibble(v) (0xf0 & (v))

#if CHAR_BIT == 8
# define lobyte(v) ((UBYTE)(v))
#else
# define lobyte(v) ((UBYTE)(0xff & (v)))
#endif
#define hibyte(v) lobyte ((UWORD)(v) >> 8u)

#if USHRT_MAX == 0xFFFF
# define loword(v) ((unsigned short)(v))
#else
# define loword(v) (0xFFFF & (unsigned)(v))
#endif
#define hiword(v) loword ((v) >> 16u)

#define MK_UWORD(hib,lob) (((UWORD)(hib) <<  8u) | (UBYTE)(lob))
#define MK_ULONG(hiw,low) (((ULONG)(hiw) << 16u) | (UWORD)(low))

/* General far pointer macros                                           */
#ifdef I86
#ifndef MK_FP

#if defined __WATCOMC__
#define MK_FP(seg,ofs) 	      (((UWORD)(seg)):>((VOID *)(ofs)))
#elif __TURBOC__ > 0x202
#define MK_FP(seg,ofs)        ((void _seg *)(seg) + (void near *)(ofs))
#else
#define MK_FP(seg,ofs)        ((void FAR *)MK_ULONG(seg, ofs))
#endif

#define pokeb(seg, ofs, b) (*(unsigned char far *)MK_FP(seg,ofs) = (b))
#define poke(seg, ofs, w) (*(unsigned far *)MK_FP(seg,ofs) = (w))
#define pokew poke
#define pokel(seg, ofs, l) (*(unsigned long far *)MK_FP(seg,ofs) = (l))
#define peekb(seg, ofs) (*(unsigned char far *)MK_FP(seg,ofs))
#define peek(seg, ofs) (*(unsigned far *)MK_FP(seg,ofs))
#define peekw peek
#define peekl(seg, ofs) (*(unsigned long far *)MK_FP(seg,ofs))

#if __TURBOC__ > 0x202
#define FP_SEG(fp)            ((unsigned)(void _seg *)(void far *)(fp))
#else
#define FP_SEG(fp)            hiword ((ULONG)(VOID FAR *)(fp))
#endif

#define FP_OFF(fp)            loword (fp)

#endif
#endif

#ifdef MC68K
#define MK_FP(seg,ofs)         ((VOID *)&(((BYTE *)(size_t)(seg))[ofs]))
#define FP_SEG(fp)             0
#define FP_OFF(fp)             ((size_t)(fp))
#endif

#ifndef _WIN32
typedef VOID (FAR ASMCFUNC * intvec) (void);
#endif

#define MK_PTR(type,seg,ofs) ((type FAR*) MK_FP (seg, ofs))
#if __TURBOC__ > 0x202
# define MK_SEG_PTR(type,seg) ((type _seg*) (seg))
#else
# define _seg FAR
# define MK_SEG_PTR(type,seg) MK_PTR (type, seg, 0)
#endif

/*
	this suppresses the warning
	unreferenced parameter 'x'
	and (hopefully) generates no code
*/
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif

#ifdef I86                      /* commandline overflow - removing /DPROTO TE */
#define PROTO
#endif

typedef const char	CStr[], *PCStr;
typedef char		Str[], *PStr;
typedef const void	*CVP;
#ifdef _WIN32
typedef const void *CVFP;
typedef void *VFP;
#else
typedef const void FAR	*CVFP;
typedef void FAR	*VFP;
#endif

#define LENGTH(x) (sizeof (x)/sizeof *(x))
#define ENDOF(x) ((x) + LENGTH (x))

/* (unsigned) modulo arithmetics trick: a<=b<=c equal to b-a<=c-a */
#define inrange(type,v,lo,hi) ((type)((v) - (lo)) <= (type)((hi) - (lo)))
#define _isdigit(c) inrange(UBYTE, c, '0', '9')
#define _islower(c) inrange(UBYTE, c, 'a', 'z')
#define _isupper(c) inrange(UBYTE, c, 'A', 'Z')

/* Fast ASCII tolower/toupper */
#define _fast_lower(ch)		((ch) | 0x20)
#define _fast_dolower(var)	((var) |= 0x20)
#define _fast_upper(ch)		((ch) & ~0x20)
#define _fast_doupper(var)	((var) &= ~0x20)
