//==============================================================================
//
// x_mint.h
//
// Copyright (C) 2000 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-08-28 - Initial Version.
//==============================================================================
//
#ifndef __X_MINT_H__
# define __X_MINT_H__

# ifndef _MINTBIND_H
#  include <mintbind.h>
# endif
# undef Finstat
# undef Foutstat
# undef Fread
# undef Fwrite


static inline long Finstat(short fd)
{
    register long rtn;

    __asm__ volatile (
        "move.w	%1, -(sp)       \n\t"
        "move.w	#0x105, -(sp)   \n\t"
        "trap		#1          \n\t"
        "addq		#4, sp      \n\t"
        "moveq.l	#1, d1      \n\t"
        "add.l		d0, d1      \n\t"
        "bvc		9f          \n\t"
        "ext.l		d0          \n\t"
        "9:                     \n\t"
        "move.l	d0, %0          \n\t"
        : "=d"(rtn)                      // output
        : "g"(fd)                        // input
        : "d0","d1","d2", "a0","a1","a2" // clobbered
    );
    return rtn;
}

static inline long Foutstat(short fd)
{
    register long rtn;

    __asm__ volatile (
        "move.w	%1, -(sp)       \n\t"
        "move.w	#0x106, -(sp)   \n\t"
        "trap		#1          \n\t"
        "addq		#4, sp      \n\t"
        "moveq.l	#1, d1      \n\t"
        "add.l		d0, d1      \n\t"
        "bvc		9f          \n\t"
        "ext.l		d0          \n\t"
        "9:                     \n\t"
        "move.l	d0, %0          \n\t"
        : "=d"(rtn)                      // output
        : "g"(fd)                        // input
        : "d0","d1","d2", "a0","a1","a2" // clobbered
    );
    return rtn;
}


static inline long Fread(short fd, long count, void * buf)
{
    register long rtn;

    __asm__ volatile (
        "move.l	%3, -(sp)       \n\t"
        "move.l	%2, -(sp)       \n\t"
        "move.w	%1, -(sp)       \n\t"
        "move.w	#0x3F, -(sp)    \n\t"
        "trap		#1          \n\t"
        "adda.w	#12, sp         \n\t"
        "move.l	d0, %0          \n\t"
        : "=d"(rtn)                      // outputs
        : "g"(fd),"g"(count),"g"(buf)    // inputs
        : "d0","d1","d2", "a0","a1","a2" // clobbered regs
    );
    return rtn;
}

static inline long Fwrite(short fd, long count, void * buf)
{
    register long rtn;

    __asm__ volatile (
        "move.l	%3, -(sp)       \n\t"
        "move.l	%2, -(sp)       \n\t"
        "move.w	%1, -(sp)       \n\t"
        "move.w	#0x40, -(sp)    \n\t"
        "trap		#1          \n\t"
        "adda.w	#12, sp         \n\t"
        "move.l	d0, %0          \n\t"
        : "=d"(rtn)                      // outputs
        : "g"(fd),"g"(count),"g"(buf)    // inputs
        : "d0","d1","d2", "a0","a1","a2" // clobbered regs
    );
    return rtn;
}


#endif /* __X_MINT_H__ */
