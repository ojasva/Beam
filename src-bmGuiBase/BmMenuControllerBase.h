/*
	BmMenuControllerBase.h
		$Id$
*/
/*************************************************************************/
/*                                                                       */
/*  Beam - BEware Another Mailer                                         */
/*                                                                       */
/*  http://www.hirschkaefer.de/beam                                      */
/*                                                                       */
/*  Copyright (C) 2002 Oliver Tappe <beam@hirschkaefer.de>               */
/*                                                                       */
/*  This program is free software; you can redistribute it and/or        */
/*  modify it under the terms of the GNU General Public License          */
/*  as published by the Free Software Foundation; either version 2       */
/*  of the License, or (at your option) any later version.               */
/*                                                                       */
/*  This program is distributed in the hope that it will be useful,      */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU    */
/*  General Public License for more details.                             */
/*                                                                       */
/*  You should have received a copy of the GNU General Public            */
/*  License along with this program; if not, write to the                */
/*  Free Software Foundation, Inc., 59 Temple Place - Suite 330,         */
/*  Boston, MA  02111-1307, USA.                                         */
/*                                                                       */
/*************************************************************************/


#ifndef _BmMenuControllerBase_h
#define _BmMenuControllerBase_h

#include <PopUpMenu.h>

#include "BmString.h"

#include "BmRosterBase.h"

#include "BmGuiBase.h"


enum {
	BM_MC_MOVE_RIGHT			= 1<<0,
	BM_MC_SKIP_FIRST_LEVEL	= 1<<1,
	BM_MC_ADD_NONE_ITEM		= 1<<2,
	BM_MC_LABEL_FROM_MARKED	= 1<<3,
	BM_MC_RADIO_MODE			= 1<<4
};

enum {
	BM_MENUITEM_SELECTED = 'bMis'
};

class BMenuItem;

typedef void (BmRosterBase::*BmRebuildMenuFunc)(BmMenuControllerBase*);

class IMPEXPBMGUIBASE BmMenuControllerBase : public BPopUpMenu
{
	typedef BPopUpMenu inherited;
	

public:

	BmMenuControllerBase( const char* label, BHandler* msgTarget, 
								 BMessage* msgTemplate,
								 BmRebuildMenuFunc fn, int32 flags=0);

	virtual ~BmMenuControllerBase();

	// native methods
	void MarkItem( const char* label);
	void ClearMark();
	//
	static BMenuItem* MarkItemInMenu( BMenu* menu, const char* label);
	static void ClearMarkInMenu( BMenu* menu);
	
	// overrides of view-base
	void AttachedToWindow();
	BPoint ScreenLocation();

	// getters:
	BHandler* MsgTarget() const;
	BMessage* MsgTemplate() 		 		{ return mMsgTemplate; }
	const BmString& Shortcuts() const 	{ return mShortcuts; }

	// setters:
	void MsgTarget( BHandler* t)  		{ mMsgTarget = t; }
	void Shortcuts( const BmString s);

protected:
	virtual void UpdateItemList();

	BHandler* mMsgTarget;
	BMessage* mMsgTemplate;
	BmString mShortcuts;
	BmRebuildMenuFunc mRebuildMenuFunc;
	int32 mFlags;
	BmString mMarkedLabel;

private:
	// Hide copy-constructor and assignment:
	BmMenuControllerBase( const BmMenuControllerBase&);
	BmMenuControllerBase operator=( const BmMenuControllerBase&);
};


#endif
