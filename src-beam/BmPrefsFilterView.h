/*
	BmPrefsFilterView.h
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


#ifndef _BmPrefsFilterView_h
#define _BmPrefsFilterView_h

#include "BmListController.h"
#include "BmFilter.h"
#include "BmPrefsView.h"

/*------------------------------------------------------------------------------*\
	BmFilterItem
		-	
\*------------------------------------------------------------------------------*/
class BmFilterItem : public BmListViewItem
{
	typedef BmListViewItem inherited;

public:
	// c'tors and d'tor:
	BmFilterItem( const BmString& key, BmListModelItem* item);
	~BmFilterItem();

	// overrides of listitem base:
	void UpdateView( BmUpdFlags flags);

private:
	// Hide copy-constructor and assignment:
	BmFilterItem( const BmFilterItem&);
	BmFilterItem operator=( const BmFilterItem&);
};



/*------------------------------------------------------------------------------*\
	BmFilterView
		-	
\*------------------------------------------------------------------------------*/
class BmFilterView : public BmListViewController
{
	typedef BmListViewController inherited;
	
public:
	// creator-func, c'tors and d'tor:
	static BmFilterView* CreateInstance( minimax minmax, int32 width, int32 height);
	BmFilterView(  minimax minmax, int32 width, int32 height);
	~BmFilterView();

	// native methods:
	BmListViewItem* CreateListViewItem( BmListModelItem* item, BMessage* archive=NULL);
	
	// overrides of controller base:
	BmString StateInfoBasename()			{ return "FilterView"; }
	void UpdateModelItem( BMessage* msg);
	BmListViewItem* AddModelItem( BmListModelItem* item);
	const char* ItemNameForCaption()		{ return "filter"; }
	CLVContainerView* CreateContainer( bool horizontal, bool vertical, 
												  bool scroll_view_corner, 
												  border_style border, 
												  uint32 ResizingMode, 
												  uint32 flags);

	// overrides of listview base:
	void MessageReceived( BMessage* msg);

private:

	// Hide copy-constructor and assignment:
	BmFilterView( const BmFilterView&);
	BmFilterView operator=( const BmFilterView&);
};



#define BM_ADD_FILTER			'bmAS'
#define BM_REMOVE_FILTER		'bmRS'
#define BM_MOVE_UP_FILTER		'bmFU'
#define BM_MOVE_DOWN_FILTER	'bmFD'
#define BM_TEST_FILTER			'bmTS'
#define BM_IS_ACTIVE_CHANGED	'bmAC'


class BmCheckControl;
class BmMultiLineTextControl;
class BmTextControl;
class MButton;
class MTabView;
/*------------------------------------------------------------------------------*\
	BmPrefsFilterView
		-	
\*------------------------------------------------------------------------------*/
class BmPrefsFilterView : public BmPrefsView {
	typedef BmPrefsView inherited;

public:
	// c'tors and d'tor:
	BmPrefsFilterView( BmFilterList* filterList, bool outbound=false);
	virtual ~BmPrefsFilterView();
	
	// native methods:
	void ShowFilter( int32 selection);

	// overrides of BmPrefsView base:
	void Initialize();
	void Activated();
	void WriteStateInfo();
	void SaveData();
	void UndoChanges();
	bool SanityCheck();

	// overrides of BView base:
	void MessageReceived( BMessage* msg);

	// getters:

	// setters:

private:
	CLVContainerView* CreateFilterListView( minimax minmax, int32 width, int32 height);

	bool mOutbound;
	BmFilterList* mFilterList;

	BmListViewController* mFilterListView;
	BmTextControl* mFilterControl;
	BmCheckControl* mIsActiveControl;
	BmMultiLineTextControl* mContentControl;
	MButton* mAddButton;
	MButton* mRemoveButton;
	MButton* mMoveUpButton;
	MButton* mMoveDownButton;
	MButton* mTestButton;
	MTabView* mTabView;

	BmRef<BmFilter> mCurrFilter;
	
	// Hide copy-constructor and assignment:
	BmPrefsFilterView( const BmPrefsFilterView&);
	BmPrefsFilterView operator=( const BmPrefsFilterView&);
};

#endif
