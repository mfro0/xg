//==============================================================================
//
// wind_grab.c
//
// Copyright (C) 2000,2001 Ralph Lowinski <AltF4@freemint.de>
//------------------------------------------------------------------------------
// 2000-12-14 - Module released for beta state.
// 2000-07-24 - Initial Version.
//==============================================================================
//
#include "window_P.h"
#include "event.h"
#include "Cursor.h"

#include <stdio.h> // printf

#include <X11/Xproto.h>


CLIENT * _WIND_PgrabClient = NULL;
WINDOW * _WIND_PgrabWindow = NULL;
CURSOR * _WIND_PgrabCursor = NULL;
CARD32   _WIND_PgrabEvents = 0ul;
BOOL     _WIND_PgrabOwnrEv = xFalse;
CARD32   _WIND_PgrabTime   = 0ul;


//------------------------------------------------------------------------------
static void
_Wind_PgrabSet (CLIENT * clnt, WINDOW * wind,
                p_CURSOR crsr, CARD32 mask, BOOL ownr, CARD32 time)
{
	if (!_WIND_PgrabClient) {
		WindMctrl (xTrue);
	}
	_WIND_PgrabClient = clnt;
	_WIND_PgrabWindow = wind;
	_WIND_PgrabEvents = mask;
	_WIND_PgrabOwnrEv = ownr;
	_WIND_PgrabTime   = time;
	if (_WIND_PgrabCursor) {
		CrsrFree (_WIND_PgrabCursor, NULL);
	}
	if ((_WIND_PgrabCursor = crsr)) {
		CrsrSelect (crsr);
	} else {
		_Wind_Cursor (_WIND_PgrabWindow);
	}
	
	if (wind != _WIND_PointerRoot) {
		WINDOW * stack[32];
		int anc;
		int top  = _Wind_PathStack (stack, &anc, _WIND_PointerRoot, wind);
		PXY r_xy = WindPointerPos (wind);
		PXY e_xy = WindPointerPos (_WIND_PointerRoot);
		EvntPoiner (stack, anc, top, e_xy, r_xy, wind->Id, NotifyGrab);
	}
}

//------------------------------------------------------------------------------
BOOL
_Wind_PgrabClear (CLIENT * clnt)
{
	if (!clnt  ||  clnt == _WIND_PgrabClient) {
		_WIND_PgrabClient = NULL;
		_WIND_PgrabWindow = NULL;
		if (_WIND_PgrabCursor) {
			CrsrFree (_WIND_PgrabCursor, NULL);
			_WIND_PgrabCursor = NULL;
		}
		WindMctrl (xFalse);
		
		return xTrue;
	}
	return xFalse;
}


//==============================================================================
//
// Callback Functions

#include "Request.h"

//------------------------------------------------------------------------------
void
RQ_GrabPointer (CLIENT * clnt, xGrabPointerReq * q)
{
	WINDOW * wind = WindFind (q->grabWindow);
	CURSOR * crsr = NULL;
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabPointer,);
	
	} else if (q->cursor && !(crsr = CrsrGet(q->cursor))) {
		Bad(Cursor, q->cursor, GrabPointer,);
	
	} else {
		ClntReplyPtr (GrabPointer, r);
		CARD32 time = (q->time ? q->time : MAIN_TimeStamp);
		
		PRINT (GrabPointer," W:%lX(W:%lX) evnt=0x%04X/%i mode=%i/%i"
		                       " C:%lX T:%lX",
		       q->grabWindow, q->confineTo, q->eventMask, q->ownerEvents,
		       q->pointerMode, q->keyboardMode, q->cursor, q->time);
		
		if (_WIND_PgrabClient  &&  _WIND_PgrabClient != clnt) {
			r->status = AlreadyGrabbed;
		
		} else if (!WindVisible (wind)) {
			r->status = GrabNotViewable;
		
		} else if (time < _WIND_PgrabTime || time > MAIN_TimeStamp) {
			r->status = GrabInvalidTime;
		
		} else {
			r->status = GrabSuccess;
		}
		ClntReply (GrabPointer,, NULL);
		
		if (r->status == GrabSuccess) {
			_Wind_PgrabSet (clnt, wind, crsr, q->eventMask, q->ownerEvents, time);
		
		} else if (crsr) {
			CrsrFree (crsr, NULL);
		}
	}
}

//------------------------------------------------------------------------------
void
RQ_ChangeActivePointerGrab (CLIENT * clnt, xChangeActivePointerGrabReq * q)
{
	PRINT (- X_ChangeActivePointerGrab," C:%lX T:%lX evnt=%04X",
	       q->cursor, q->time, q->eventMask);
}

//------------------------------------------------------------------------------
void
RQ_UngrabPointer (CLIENT * clnt, xUngrabPointerReq * q)
{
	CARD32 wid = (_WIND_PgrabWindow ? _WIND_PgrabWindow->Id : 0);
	
	if ((!q->id || (q->id >= _WIND_PgrabTime && q->id <= MAIN_TimeStamp))
	    && _Wind_PgrabClear (clnt)) {
		PRINT (UngrabPointer," W:%lX T:%lX", wid, q->id);
		_Wind_Cursor (_WIND_PointerRoot);
	
	} else {
		PRINT (UngrabPointer," W:%lX T:%lX ignored", wid, q->id);
	}
}

//------------------------------------------------------------------------------
void
RQ_GrabButton (CLIENT * clnt, xGrabButtonReq * q)
{
	PRINT (- X_GrabButton," W:%lX(W:%lX) evnt=0x%04X/%i mode=%i/%i"
	                      " crsr=0x%lX but=%i/%04X",
	       q->grabWindow, q->confineTo, q->eventMask, q->ownerEvents,
	       q->pointerMode, q->keyboardMode, q->cursor, q->button, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_UngrabButton (CLIENT * clnt, xUngrabButtonReq * q)
{
	PRINT (- X_UngrabButton," W:%lX but=%i/%04X",
	       q->grabWindow, q->button, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_GrabKeyboard (CLIENT * clnt, xGrabKeyboardReq * q)
{
	WINDOW * wind = WindFind (q->grabWindow);
	
	if (!wind) {
		Bad(Window, q->grabWindow, GrabKeyboard,);
	
	} else {
		ClntReplyPtr (GrabKeyboard, r);
		
		PRINT (- X_GrabKeyboard," W:%lX ownr=%i mode=%i/%i T:%lX",
		       q->grabWindow, q->ownerEvents,
		       q->pointerMode, q->keyboardMode, q->time);
		
		r->status = GrabSuccess;
		
		ClntReply (GrabKeyboard,, NULL);
	}
}

//------------------------------------------------------------------------------
void
RQ_UngrabKeyboard (CLIENT * clnt, xUngrabKeyboardReq * q)
{
	PRINT (- X_UngrabKeyboard," T:%lX", q->id);
}

//------------------------------------------------------------------------------
void
RQ_GrabKey (CLIENT * clnt, xGrabKeyReq * q)
{
	PRINT (- X_GrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}

//------------------------------------------------------------------------------
void
RQ_UngrabKey (CLIENT * clnt, xUngrabKeyReq * q)
{
	PRINT (- X_UngrabKey," W:%lX key=%i/%04x",
	       q->grabWindow, q->key, q->modifiers);
}


//------------------------------------------------------------------------------
void
RQ_SetInputFocus (CLIENT * clnt, xSetInputFocusReq * q)
{
	PRINT (- X_SetInputFocus," W:%lX(%i) T:%lX", q->focus, q->revertTo, q->time);
}

//------------------------------------------------------------------------------
void
RQ_GetInputFocus (CLIENT * clnt, xGetInputFocusReq * q)
{
	ClntReplyPtr (GetInputFocus, r);
	
//	PRINT (- X_GetInputFocus," ");
	
	r->revertTo = None;
	r->focus    = PointerRoot; //None;
	
	ClntReply (GetInputFocus,, "w");
}
