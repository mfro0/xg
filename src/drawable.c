//==============================================================================
//
// drawable.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-06-05 - Initial Version.
//==============================================================================
//
#include "main.h"
#include "tools.h"
#include "clnt.h"
#include "pixmap.h"
#include "window.h"
#include "gcontext.h"
#include "font.h"
#include "grph.h"
#include "x_gem.h"
#include "gemx.h"

#include <stdio.h>

#include <X11/X.h>


//==============================================================================
void
DrawDelete (p_DRAWABLE draw, p_CLIENT clnt)
{
	if (draw.p->isWind) {
		WINDOW * wind = draw.Window;
		while (RID_Match (clnt->Id, wind->Parent->Id)) {
			wind = wind->Parent;
		}
		WindDelete (wind, clnt);
	
	} else {
		PmapFree (draw.Pixmap, clnt);
	}
}


//------------------------------------------------------------------------------
static inline GRECT *
SizeToRCT (GRECT * dst, const short * src)
{
	__asm__ volatile ("
		clr.l		d0;
		move.l	(%0), d1;
		movem.l	d0/d1, (%1);
		"
		:                   // output
		: "a"(src),"a"(dst) // input
		: "d0","d1"         // clobbered
	);
	return dst;
}

//------------------------------------------------------------------------------
static inline CARD16
SizeToClp (GRECT * dst, GRECT * clip, CARD16 n_clip, const short * src)
{
	CARD16 nClp = 0;
	
	while (n_clip--) {
		if (clip->x <= 0) {
			 dst->x = 0;
			 dst->w = (clip->w <= src[0] ? clip->w : src[0]) + clip->x;
		} else {
			dst->x = clip->x;
			if (clip->w + clip->x > src[0]) dst->w = src[0] - clip->x;
			else                            dst->w = clip->w;
		}
		if (clip->y <= 0) {
			 dst->y = 0;
			 dst->h = (clip->h <= src[1] ? clip->h : src[1]) + clip->y;
		} else {
			dst->y = clip->y;
			if (clip->h + clip->y > src[1]) dst->h = src[1] - clip->y;
			else                            dst->h = clip->h;
		}
		if (dst->w > 0  &&  dst->h > 0) {
			dst++;
			nClp++;
		}
		clip++;
	}
	return nClp;
}


//------------------------------------------------------------------------------
static inline BOOL
gc_mode (CARD32 * color, BOOL * set, int hdl, const GC * gc)
{
	BOOL  fg;
	short m;
	
	switch (gc->Function) {
		case GXequiv:                                            // !src XOR  dst
		case GXset:                                              //  1
		case GXcopy:         fg = xTrue;  m = MD_REPLACE; break; //  src
		
		case GXcopyInverted:                                     // !src
		case GXclear:        fg = xFalse; m = MD_REPLACE; break; //  0
		
		case GXxor:          fg = xTrue;  m = MD_XOR;     break; //  src XOR  dst
		
		case GXnor:                                              // !src AND !dst
		case GXinvert:       fg = xFalse; m = MD_XOR;     break; //          !dst
		
		case GXnand:                                             // !src OR  !dst
		case GXorReverse:                                        //  src OR  !dst
		case GXor:                                               //  src OR   dst
		case GXandReverse:   fg = xTrue;  m = MD_TRANS;   break; //  src AND !dst
		
		
		case GXorInverted:                                    // !src OR   dst
		case GXandInverted:                                   // !src AND  dst
		case GXand:                                           //  src AND  dst
		case GXnoop:                                          //           dst
		default:             return xFalse;
	}
	if (fg) {
		*color = gc->Foreground;
	} else {
		*color = gc->Background;
		*set   = xTrue;
	}
	vswr_mode (hdl, m);
	
	return xTrue;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_GetGeometry (CLIENT * clnt, xGetGeometryReq * q)
{
	DRAWABLE * draw = NULL;
	
	if ((q->id & ~RID_MASK) && !(draw = DrawFind(q->id).p)) {
		Bad(Drawable, q->id, GetGeometry,);
	
	} else {
		ClntReplyPtr (GetGeometry, r);
		r->root = ROOT_WINDOW;
		
		if (!draw) {
			wind_get_work (q->id & 0x7FFF, (GRECT*)&r->x);
			r->depth       = WIND_Root.Depth;
			r->x          -= WIND_Root.Rect.x +1;
			r->y          -= WIND_Root.Rect.y +1;
			r->borderWidth = 1;
		
		} else {
			r->depth = draw->Depth;
			if (draw->isWind) {
				WINDOW * wind = (WINDOW*)draw;
				*(GRECT*)&r->x = wind->Rect;
				r->x -= wind->BorderWidth;
				r->y -= wind->BorderWidth;
				r->borderWidth = wind->BorderWidth;
			} else {
				PIXMAP * pmap = (PIXMAP*)draw;
				SizeToRCT ((GRECT*)&r->x, &pmap->W);
				r->borderWidth   = 0;
			}
			
			DEBUG (GetGeometry," 0x%lX", q->id);
		}
		
		ClntReply (GetGeometry,, "wR.");
	}
}


//------------------------------------------------------------------------------
void
RQ_FillPoly (CLIENT * clnt, xFillPolyReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xFillPolyReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, FillPoly,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, FillPoly,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (FillPoly," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				vs_clip_r    (hdl, sect++);
				v_fillarea_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (FillPoly," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyArc (CLIENT * clnt, xPolyArcReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyArcReq)) / sizeof(xArc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyArc,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyArc,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		xArc  * arc = (xArc*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		PXY     orig;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyArc," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			clnt->Fnct->shift_arc (&orig, arc, len, ArcPieSlice);
			if (set) {
				vsl_width (hdl, gc->LineWidth);
				vsl_color (hdl, color);
			}
			do {
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; ++i) {
					v_ellarc (hdl, arc[i].x, arc[i].y,
					          arc[i].width, arc[i].height,
					          arc[i].angle1, arc[i].angle2);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyFillArc (CLIENT * clnt, xPolyFillArcReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyFillArcReq)) / sizeof(xArc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyFillArc,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyFillArc,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		xArc  * arc = (xArc*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		PXY     orig;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyFillArc," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			clnt->Fnct->shift_arc (&orig, arc, len, gc->ArcMode);
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; ++i) {
					v_ellpie (hdl, arc[i].x, arc[i].y,
					          arc[i].width, arc[i].height,
					          arc[i].angle1, arc[i].angle2);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyLine (CLIENT * clnt, xPolyLineReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyLineReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyLine,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyLine,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (PolyLine," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsl_color (hdl, color);
				vsl_width (hdl, gc->LineWidth);
			}
			do {
				vs_clip_r (hdl, sect++);
				v_pline_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (PolyLine," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyPoint (CLIENT * clnt, xPolyPointReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyPointReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyPoint,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyPoint,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, q->coordMode);
				v_hide_c (hdl);
			}
			DEBUG (PolyPoint," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			clnt->Fnct->shift_pnt (NULL, pxy, len, q->coordMode);
			if (draw.p->Depth == 1) {
				PmapDrawPoints (draw.Pixmap, gc, pxy, len);
				nClp = 0;
			
			} else {
				if (gc->ClipNum > 0) {
					sect = alloca (sizeof(GRECT) * gc->ClipNum);
					nClp = SizeToClp (sect,
					                  gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
				} else {
					sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
					nClp = 1;
				}
				set = (draw.Pixmap->Vdi > 0);
				hdl = PmapVdi (draw.Pixmap, gc, xFalse);
			}
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsm_color (hdl, color);
			}
			do {
				vs_clip_r   (hdl, sect++);
				v_pmarker_p (hdl, len, pxy);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	
	} else {
		PRINT (PolyPoint," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyFillRectangle (CLIENT * clnt, xPolyFillRectangleReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyFillRectangleReq))
	                / sizeof(GRECT);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyFillRectangle,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyFillRectangle,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_r2p (&orig, rec, len);
				v_hide_c (hdl);
			}
			DEBUG (PolyFillRectangle," P:%lX G:%lX (%lu)",
			       q->drawable, q->gc, len);
		
		} else { // Pixmap
			/*if (draw.p->Depth == 1) {   // disabled
				if (clnt->DoSwap) {
					size_t  num = len;
					GRECT * rct = rec;
					while (num--) {
						SwapRCT(rct, rct);
						rct++;
					}
				}
				PmapFillRects (draw.Pixmap, gc, rec, len);
				nClp = 0;
			
			} else*/ {
				if (gc->ClipNum > 0) {
					sect = alloca (sizeof(GRECT) * gc->ClipNum);
					nClp = SizeToClp (sect,
					                  gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
				} else {
					sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
					nClp = 1;
				}
				clnt->Fnct->shift_r2p (NULL, rec, len);
				set = (draw.Pixmap->Vdi > 0);
				hdl = PmapVdi (draw.Pixmap, gc, xFalse);
			}
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsf_color (hdl, color);
			}
			do {
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; v_bar_p (hdl, (PXY*)&rec[i++]));
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	} else {
		PRINT (PolyFillRectangle," D:%lX G:%lX (0)",q->drawable, q->gc);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyRectangle (CLIENT * clnt, xPolyRectangleReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolyRectangleReq))
	                / sizeof(GRECT);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyRectangle,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyRectangle,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		GRECT * rec = (GRECT*)(q +1);
		short   hdl = GRPH_Vdi;
		PXY     orig;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				v_hide_c (hdl);
			}
			DEBUG (PolyRectangle," P:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			*(long*)&orig = 0;
			set  = (draw.Pixmap->Vdi > 0);
			hdl  = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			short d = vsl_width (hdl, gc->LineWidth);
			if (set) {
				vsl_color (hdl, color);
			}
			if (clnt->DoSwap) {
				size_t  n = len;
				GRECT * r = rec;
				while (n--) {
					SwapRCT(r, r);
					r++;
				}
			}	
			do {
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; ++i) {
					PXY p[5];
					p[0].x = p[3].x = p[4].x = rec[i].x + orig.x;
					p[1].x = p[2].x = p[0].x + rec[i].w -1;
					p[0].y = p[1].y          = rec[i].y + orig.y;
					p[2].y = p[3].y = p[0].y + rec[i].h -1;
					p[4].y          = p[0].y + d;
					v_pline_p (hdl, 5, p);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolySegment (CLIENT * clnt, xPolySegmentReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	size_t     len  = ((q->length *4) - sizeof (xPolySegmentReq)) / sizeof(PXY);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolySegment,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolySegment,);
	
	} else if (len  &&  gc->ClipNum >= 0) {
		BOOL    set = xTrue;
		PXY   * pxy = (PXY*)(q +1);
		short   hdl = GRPH_Vdi;
		GRECT * sect;
		CARD16  nClp;
		CARD32  color;
		
		if (draw.p->isWind) {
			PXY orig;
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				clnt->Fnct->shift_pnt (&orig, pxy, len, CoordModeOrigin);
				v_hide_c (hdl);
			}
			DEBUG (PolySegment," W:%lX G:%lX (%lu)", q->drawable, q->gc, len);
		
		} else { // Pixmap
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			clnt->Fnct->shift_pnt (NULL, pxy, len, CoordModeOrigin);
			set = (draw.Pixmap->Vdi > 0);
			hdl = PmapVdi (draw.Pixmap, gc, xFalse);
		}
		if (nClp && gc_mode (&color, &set, hdl, gc)) {
			if (set) {
				vsl_color (hdl, color);
				vsl_width (hdl, gc->LineWidth);
			}
			do {
				int i;
				vs_clip_r (hdl, sect++);
				for (i = 0; i < len; i += 2) {
					v_pline_p (hdl, 2, &pxy[i]);
				}
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
static void
_Image_Text (p_DRAWABLE draw, GC * gc,
             BOOL is8N16, void * text, short len, PXY * pos)
{
	short   hdl = GRPH_Vdi;
	short   arr[len];
	GRECT * sect;
	PXY     orig;
	CARD16  nClp;
	
	if (draw.p->isWind) {
		nClp = WindClipLock (draw.Window, 0,
		                     gc->ClipRect, gc->ClipNum, &orig, &sect);
		if (nClp) {
			int dmy;
			orig.x += pos->x;
			orig.y += pos->y;
			vst_font    (hdl, gc->FontIndex);
			vst_effects (hdl, gc->FontEffects);
			if (gc->FontWidth) {
				vst_height (hdl, gc->FontPoints, &dmy,&dmy,&dmy,&dmy);
				vst_width  (hdl, gc->FontWidth,  &dmy,&dmy,&dmy,&dmy);
			} else {
				vst_point  (hdl, gc->FontPoints, &dmy, &dmy, &dmy, &dmy);
			}
			v_hide_c (hdl);
		}
		
	} else { // Pixmap
		if (gc->ClipNum > 0) {
			sect = alloca (sizeof(GRECT) * gc->ClipNum);
			nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
		} else {
			sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
			nClp = 1;
		}
		orig = *pos;
		hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
	}
	if (nClp) {
		if (is8N16) FontLatin1_C (arr, text, len);
		else        FontLatin1_W (arr, text, len);
		do {
			vs_clip_r (hdl, sect++);
			if (gc->Background == WHITE) {
				vswr_mode   (hdl, MD_REPLACE);
			} else {
				vswr_mode   (hdl, MD_ERASE);
				vst_color   (hdl, gc->Background);
				v_gtext_arr (hdl, &orig, len, arr);
				vswr_mode   (hdl, MD_TRANS);
			}
			vst_color   (hdl, gc->Foreground);
			v_gtext_arr (hdl, &orig, len, arr);
		} while (--nClp);
		vs_clip_r (hdl, NULL);
		
		if (draw.p->isWind) {
			v_show_c (hdl, 1);
			WindClipOff();
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_ImageText8 (CLIENT * clnt, xImageTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, ImageText8,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, ImageText8,);
	
	} else if (q->nChars  &&  gc->ClipNum >= 0) {
		DEBUG (ImageText8," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Image_Text (draw, gc, xTrue, (char*)(q +1), q->nChars, (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
void
RQ_ImageText16 (CLIENT * clnt, xImageTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, ImageText16,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, ImageText16,);
	
	} else if (q->nChars  &&  gc->ClipNum >= 0) {
		DEBUG (ImageText16," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Image_Text (draw, gc, xFalse, (char*)(q +1), q->nChars, (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
static void
_Poly_Text (p_DRAWABLE draw, GC * gc, BOOL is8N16, xTextElt * t, PXY * pos)
{
	if (t->len) {
		short   hdl = GRPH_Vdi;
		short   arr[t->len];
		GRECT * sect;
		PXY     orig;
		CARD16  nClp;
		
		if (draw.p->isWind) {
			nClp = WindClipLock (draw.Window, 0,
			                     gc->ClipRect, gc->ClipNum, &orig, &sect);
			if (nClp) {
				int dmy;
				orig.x += pos->x;
				orig.y += pos->y;
				vst_font    (hdl, gc->FontIndex);
				vst_color   (hdl, gc->Foreground);
				vst_effects (hdl, gc->FontEffects);
				if (gc->FontWidth) {
					vst_height (hdl, gc->FontPoints, &dmy,&dmy,&dmy,&dmy);
					vst_width  (hdl, gc->FontWidth,  &dmy,&dmy,&dmy,&dmy);
				} else {
					vst_point  (hdl, gc->FontPoints, &dmy, &dmy, &dmy, &dmy);
				}
				v_hide_c (hdl);
			}
		
		} else { // Pixmap
			DEBUG (PolyText8," P:%lX G:%lX (%i,%i)",
			       q->drawable, q->gc, q->x, q->y);
			if (gc->ClipNum > 0) {
				sect = alloca (sizeof(GRECT) * gc->ClipNum);
				nClp = SizeToClp (sect, gc->ClipRect, gc->ClipNum, &draw.Pixmap->W);
			} else {
				sect = SizeToRCT (alloca (sizeof(GRECT)), &draw.Pixmap->W);
				nClp = 1;
			}
			orig = *pos;
			hdl  = PmapVdi (draw.Pixmap, gc, xTrue);
		}
		if (nClp) {
			if (is8N16) FontLatin1_C (arr,  (char*)(t +1), t->len);
			else        FontLatin1_W (arr, (short*)(t +1), t->len);
			vswr_mode (hdl, MD_TRANS);
			do {
				vs_clip_r (hdl, sect++);
				v_gtext_arr (hdl, &orig, t->len, arr);
			} while (--nClp);
			vs_clip_r (hdl, NULL);
			
			if (draw.p->isWind) {
				v_show_c (hdl, 1);
				WindClipOff();
			}
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyText8 (CLIENT * clnt, xPolyTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyText8,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyText8,);
	
	} else if (gc->ClipNum >= 0) {
		DEBUG (PolyText8," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Poly_Text (draw, gc, xTrue, (xTextElt*)(q +1), (PXY*)&q->x);
	}
}

//------------------------------------------------------------------------------
void
RQ_PolyText16 (CLIENT * clnt, xPolyTextReq * q)
{
	p_DRAWABLE draw = DrawFind (q->drawable);
	p_GC       gc   = GcntFind (q->gc);
	
	if (!draw.p) {
		Bad(Drawable, q->drawable, PolyText16,);
		
	} else if (!gc) {
		Bad(GC, q->drawable, PolyText16,);
	
	} else if (gc->ClipNum >= 0) {
		DEBUG (PolyText16," %c:%lX G:%lX (%i,%i)",
		       (draw.p->isWind ? 'W' : 'P'), q->drawable, q->gc, q->x, q->y);
		
		_Poly_Text (draw, gc, xFalse, (xTextElt*)(q +1), (PXY*)&q->x);
	}
}
