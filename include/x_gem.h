//==============================================================================
//
// x_gem.h -- extensions to the gem-lib.
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#ifndef __X_GEM_H__
# define __X_GEM_H__

#include "types.h"
#include <gem.h>

#if !defined(__GEMLIB__) || (__GEMLIB_MAJOR__ == 0 && __GEMLIB_MINOR__ < 42)
# error You need a more recent GEMLib (at least 0.42.0)!
#endif


#define K_LOCK    0x10   // bitmask returned by Kbshift()
#define K_ALTGR   0x80


#define ApplId(void) (aes_global[2])

#define wind_get_curr(h,r)        wind_get_grect (h, WF_CURRXYWH,      r)
#define wind_get_first(h,r)       wind_get_grect (h, WF_FIRSTXYWH,     r)
#define wind_get_full(h,r)        wind_get_grect (h, WF_FULLXYWH,      r)
#define wind_get_next(h,r)        wind_get_grect (h, WF_NEXTXYWH,      r)
#define wind_get_prev(h,r)        wind_get_grect (h, WF_PREVXYWH,      r)
#define wind_get_uniconify(h,r)   wind_get_grect (h, WF_UNICONIFYXYWH, r)
#define wind_get_work(h,r)        wind_get_grect (h, WF_WORKXYWH,      r)
short wind_get_one  (int WindowHandle, int What);
#define wind_get_bottom()         wind_get_one   (0, WF_BOTTOM)
#define wind_get_top()            wind_get_one   (0, WF_TOP)
#define wind_get_bevent(h)        wind_get_one   (h, WF_BEVENT)
#define wind_get_hslide(h)        wind_get_one   (h, WF_HSLIDE)
#define wind_get_hslsize(h)       wind_get_one   (h, WF_HSLSIZE)
#define wind_get_kind(h)          wind_get_one   (h, WF_KIND)
#define wind_get_vslide(h)        wind_get_one   (h, WF_VSLIDE)
#define wind_get_vslsize(h)       wind_get_one   (h, WF_VSLSIZE)
#define wind_set_curr(h,r)        wind_set_grect (h, WF_CURRXYWH, r)
short wind_set_proc (int WindowHandle, CICONBLK *icon);
#define WIND_UPDATE_BEG           wind_update (BEG_UPDATE); {
#define WIND_UPDATE_END           } wind_update (END_UPDATE);


#define SHUT_COMPLETED   60
#define RESCHG_COMPLETED 61
#define AP_DRAGDROP      63
#define SH_WDRAW         72
#define SC_CHANGED       80
#define PRN_CHANGED      82
#define FNT_CHANGED      83
#define COLORS_CHANGED   84
#define CH_EXIT          90


#endif /* __X_GEM_H__ */
