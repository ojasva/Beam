/*
	BmPrefsFilterChainView.cpp
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

#include <Alert.h>
#include <MenuItem.h>
#include <PopUpMenu.h>


//#include <liblayout/LayeredGroup.h>
#include <liblayout/HGroup.h>
#include <liblayout/LayeredGroup.h>
#include <liblayout/MButton.h>
#include <liblayout/MPlayBW.h>
#include <liblayout/MPlayFW.h>
#include <liblayout/MFFWD.h>
#include <liblayout/MStringView.h>
#include <liblayout/Space.h>
#include <liblayout/VGroup.h>

#include "BubbleHelper.h"
#include "UserResizeSplitView.h"

#include "BmLogHandler.h"
#include "BmPrefs.h"
#include "BmPrefsFilterView.h"
#include "BmPrefsFilterChainView.h"
#include "BmTextControl.h"
#include "BmUtil.h"

#define BM_CHAIN_FILTER			M_PLAYBW_SELECTED
#define BM_UNCHAIN_FILTER		M_PLAYFW_SELECTED
#define BM_EMPTY_CHAIN			M_FFWD_SELECTED

/********************************************************************************\
	BmFilterChainItem
\********************************************************************************/

enum Columns {
	COL_KEY = 0
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterChainItem::BmFilterChainItem( const BmString& key, BmListModelItem* _item)
	:	inherited( key, _item, false)
{
	UpdateView( UPD_ALL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterChainItem::~BmFilterChainItem() { 
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmFilterChainItem::UpdateView( BmUpdFlags flags) {
	inherited::UpdateView( flags);
	BmFilterChain* chain( dynamic_cast<BmFilterChain*>( ModelItem()));
	if (flags & UPD_ALL) {
		BmListColumn cols[] = {
			{ chain->Key().String(),			false },
			{ NULL, false }
		};
		SetTextCols( 0, cols);
	}
}



/********************************************************************************\
	BmFilterChainView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterChainView* BmFilterChainView::CreateInstance( minimax minmax, int32 width, int32 height) {
	return new BmFilterChainView( minmax, width, height);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterChainView::BmFilterChainView( minimax minmax, int32 width, int32 height)
	:	inherited( minmax, BRect(0,0,width-1,height-1), "Beam_FilterView", 
					  B_SINGLE_SELECTION_LIST, 
					  false, true, true, false)
{
	int32 flags = 0;
	SetViewColor( B_TRANSPARENT_COLOR);
	if (ThePrefs->GetBool("StripedListView"))
		SetStripedBackground( true);

	Initialize( BRect( 0,0,width-1,height-1),
					B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
					B_FOLLOW_TOP_BOTTOM, true, true, true, B_FANCY_BORDER);

	AddColumn( new CLVColumn( "Name", 250.0, flags, 50.0));

	SetSortFunction( CLVEasyItem::CompareItems);
	SetSortKey( COL_KEY);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterChainView::~BmFilterChainView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmFilterChainView::CreateListViewItem( BmListModelItem* item,
																		 BMessage*) {
	return new BmFilterChainItem( item->Key(), item);
}

/*------------------------------------------------------------------------------*\
	CreateContainer()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmFilterChainView::CreateContainer( bool horizontal, bool vertical, 
													  					bool scroll_view_corner, 
												  						border_style border, 
																		uint32 ResizingMode, 
																		uint32 flags) 
{
	return new BmCLVContainerView( fMinMax, this, ResizingMode, flags, horizontal, 
											 vertical, scroll_view_corner, border, mShowCaption,
											 mShowBusyView, 
											 be_plain_font->StringWidth(" 99 filter-chains "));
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmFilterChainView::AddModelItem( BmListModelItem* item) {
	BmListViewItem* viewItem = inherited::AddModelItem( item);
	if (viewItem) {
		Select( IndexOf(viewItem));
		ScrollToSelection();
	}
	return viewItem;
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmFilterChainView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			default:
				inherited::MessageReceived( msg);
		}
	} catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("FilterChainView:\n\t") << err.what());
	}
}



/********************************************************************************\
	BmChainedFilterItem
\********************************************************************************/

enum Columns2 {
	COL2_POS = 0,
	COL2_KEY,
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmChainedFilterItem::BmChainedFilterItem( const BmString& key, BmListModelItem* _item)
	:	inherited( key, _item, false)
{
	UpdateView( UPD_ALL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmChainedFilterItem::~BmChainedFilterItem() { 
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmChainedFilterItem::UpdateView( BmUpdFlags flags) {
	inherited::UpdateView( flags);
	BmChainedFilter* filter( dynamic_cast<BmChainedFilter*>( ModelItem()));
	if (flags & UPD_ALL) {
		BmString pos;
		pos << filter->Position();
		BmListColumn cols[] = {
			{ filter->Key().String(),					true },
			{ filter->FilterName().String(),			false },
			{ NULL, false }
		};
		SetTextCols( 0, cols);
	}
}



/********************************************************************************\
	BmChainedFilterView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmChainedFilterView* BmChainedFilterView::CreateInstance( minimax minmax, int32 width, int32 height) {
	return new BmChainedFilterView( minmax, width, height);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmChainedFilterView::BmChainedFilterView( minimax minmax, int32 width, int32 height)
	:	inherited( minmax, BRect(0,0,width-1,height-1), "Beam_ChainedFilterView", 
					  B_SINGLE_SELECTION_LIST, 
					  false, true, true, false)
{
	int32 flags = 0;
	SetViewColor( B_TRANSPARENT_COLOR);
	if (ThePrefs->GetBool("StripedListView"))
		SetStripedBackground( true);

	Initialize( BRect( 0,0,width-1,height-1),
					B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
					B_FOLLOW_TOP_BOTTOM, true, true, true, B_FANCY_BORDER);

	AddColumn( new CLVColumn( "Pos", 40.0, CLV_SORT_KEYABLE|CLV_RIGHT_JUSTIFIED|flags, 40.0));
	AddColumn( new CLVColumn( "Name", 250.0, flags, 200.0));

	SetSortFunction( CLVEasyItem::CompareItems);
	SetSortKey( COL2_POS);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmChainedFilterView::~BmChainedFilterView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmChainedFilterView::CreateListViewItem( BmListModelItem* item,
																			BMessage*) {
	return new BmChainedFilterItem( item->Key(), item);
}

/*------------------------------------------------------------------------------*\
	CreateContainer()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmChainedFilterView::CreateContainer( bool horizontal, bool vertical, 
												  						  bool scroll_view_corner, 
														  				  border_style border, 
																		  uint32 ResizingMode, 
																		  uint32 flags) 
{
	return new BmCLVContainerView( fMinMax, this, ResizingMode, flags, horizontal, 
											 vertical, scroll_view_corner, border, false,
											 false);
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmChainedFilterView::AddModelItem( BmListModelItem* item) {
	BmListViewItem* viewItem = inherited::AddModelItem( item);
	if (viewItem) {
		Select( IndexOf(viewItem));
		ScrollToSelection();
	}
	return viewItem;
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmChainedFilterView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			default:
				inherited::MessageReceived( msg);
		}
	} catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("ChainedFilterView:\n\t") << err.what());
	}
}



/********************************************************************************\
	BmPrefsFilterChainView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsFilterChainView::BmPrefsFilterChainView() 
	:	inherited( "Filter-Chains ")
{
	MBorder* borderl = NULL;
	MBorder* borderr = NULL;
	MView* view = 
		new VGroup(
			new HGroup(
				CreateFilterChainListView( minimax(200,120,1E5,1E5), 200, 200),
				new Space( minimax(5,0,5,1E5)),
				new VGroup(
					mAddButton = new MButton("Add Chain", new BMessage(BM_ADD_CHAIN), this),
					mRemoveButton = new MButton("Remove Chain", new BMessage( BM_REMOVE_CHAIN), this),
					new Space(),
					0
				),
				0
			),
			new HGroup(
				minimax( 400,120),
				borderl = new MBorder( M_LABELED_BORDER, 10, (char*)"Filter-Chain Info",
					new VGroup(
						new HGroup( 
							mChainControl = new BmTextControl( "Filter-Chain name:"),
							0
						),
						CreateChainedFilterListView( minimax(200,60,1E5,1E5), 200, 200),
						new HGroup(
								mMoveUpButton = new MButton( "Move Up", new BMessage( BM_MOVE_UP_FILTER), this, minimax(-1,-1,-1,-1)),
								new Space(),
								mMoveDownButton = new MButton( "Move Down", new BMessage( BM_MOVE_DOWN_FILTER), this, minimax(-1,-1,-1,-1)),
								0
						),
						0
					)
				),
				new Space( minimax(10, 0, 10, 1E5)),
				new VGroup( 
					new Space(),
					mAddFilterButton = new MPlayBW( this),
					mRemoveFilterButton = new MPlayFW( this),
					mEmptyChainButton = new MFFWD( this),
					new Space(),
					0
				),
				new Space( minimax(10, 0, 10, 1E5)),
				borderr = new MBorder( M_LABELED_BORDER, 10, (char*)"Available Filters",
					CreateAvailableFilterListView( minimax(200,60,1E5,1E5), 200, 200)
				),
				0
			),
			0
		);

	borderl->ct_mpm = minimax(250,250);
	borderr->ct_mpm = minimax(250,250);

	mGroupView->AddChild( dynamic_cast<BView*>(view));
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsFilterChainView::~BmPrefsFilterChainView() {
	TheBubbleHelper.SetHelp( mFilterChainListView, NULL);
	TheBubbleHelper.SetHelp( mChainedFilterListView, NULL);
	TheBubbleHelper.SetHelp( mAvailableFilterListView, NULL);
	TheBubbleHelper.SetHelp( mMoveUpButton, NULL);
	TheBubbleHelper.SetHelp( mMoveDownButton, NULL);
	TheBubbleHelper.SetHelp( mChainControl, NULL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::Initialize() {
	inherited::Initialize();

/*
	TheBubbleHelper.SetHelp( mFilterListView, "This listview shows every filter you have defined.");
	TheBubbleHelper.SetHelp( mTestButton, "Here you can check the syntax of the SIEVE-script.");
*/
	TheBubbleHelper.SetHelp( mChainControl, "Here you can enter a name for this filter-chain.\nThis name is used to identify this filter-chain in Beam.");

	mChainControl->SetTarget( this);

	mFilterChainListView->SetSelectionMessage( new BMessage( BM_SELECTION_CHANGED));
	mFilterChainListView->SetTarget( this);
	mFilterChainListView->StartJob( TheFilterChainList.Get());

	mChainedFilterListView->SetSelectionMessage( new BMessage( BM_CHAINED_SELECTION_CHANGED));
	mChainedFilterListView->SetTarget( this);

	mAvailableFilterListView->SetSelectionMessage( new BMessage( BM_AVAILABLE_SELECTION_CHANGED));
	mAvailableFilterListView->SetTarget( this);
	mAvailableFilterListView->StartJob( TheFilterList.Get());

	UpdateState();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::Activated() {
	inherited::Activated();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::WriteStateInfo() {
	mFilterChainListView->WriteStateInfo();
	mChainedFilterListView->WriteStateInfo();
	mAvailableFilterListView->WriteStateInfo();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmPrefsFilterChainView::SanityCheck() {
	return true;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::SaveData() {
	TheFilterChainList->Store();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::UndoChanges() {
	TheFilterChainList->Cleanup();
	TheFilterChainList->StartJobInThisThread();
	UpdateState();
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			case BM_SELECTION_CHANGED: {
				int32 selection = mFilterChainListView->CurrentSelection( 0);
				BmFilterChainItem* filterChainItem 
					= (selection != -1)
							? dynamic_cast<BmFilterChainItem*>( mFilterChainListView->ItemAt( selection))
							: NULL;
				mCurrFilterChain 
					= filterChainItem
							? dynamic_cast<BmFilterChain*>( filterChainItem->ModelItem())
							: NULL;
				if (mCurrFilterChain) {
					mChainedFilterListView->StartJob( mCurrFilterChain.Get(), false);
				} else
					mChainedFilterListView->DetachModel();
				UpdateState();
				break;
			}
			case BM_CHAINED_SELECTION_CHANGED: {
				int32 selection = mChainedFilterListView->CurrentSelection( 0);
				BmChainedFilterItem* chainedFilterItem 
					= (selection != -1)
							? dynamic_cast<BmChainedFilterItem*>( mChainedFilterListView->ItemAt( selection))
							: NULL;
				mCurrChainedFilter 
					= chainedFilterItem
							? dynamic_cast<BmChainedFilter*>( chainedFilterItem->ModelItem())
							: NULL;
				UpdateState();
				break;
			}
			case BM_AVAILABLE_SELECTION_CHANGED: {
				int32 selection  = mAvailableFilterListView->CurrentSelection( 0);
				BmFilterItem* availableFilterItem
					= (selection != -1)
							? dynamic_cast<BmFilterItem*>( mAvailableFilterListView->ItemAt( selection))
							: NULL;
				mCurrAvailableFilter
					= availableFilterItem
						 	? dynamic_cast<BmFilter*>( availableFilterItem->ModelItem())
						 	: NULL;
				UpdateState();
				break;
			}
			case BM_TEXTFIELD_MODIFIED: {
				BView* srcView = NULL;
				msg->FindPointer( "source", (void**)&srcView);
				BmTextControl* source = dynamic_cast<BmTextControl*>( srcView);
				if (mCurrFilterChain && source == mChainControl) {
					TheFilterChainList->RenameItem( mCurrFilterChain->Key(), mChainControl->Text());
					NoticeChange();
				}
				break;
			}
			case BM_ADD_CHAIN: {
				BmString key( "new filter-chain");
				for( int32 i=1; TheFilterChainList->FindItemByKey( key); ++i) {
					key = BmString("new filter-chain_")<<i;
				}
				TheFilterChainList->AddItemToList( new BmFilterChain( key.String(), 
																						TheFilterChainList.Get()));
				mChainControl->MakeFocus( true);
				mChainControl->TextView()->SelectAll();
				NoticeChange();
				break;
			}
			case BM_REMOVE_CHAIN: {
				int32 buttonPressed;
				if (msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, ask user about it:
					BAlert* alert = new BAlert( "Remove Filter-Chain", 
														 (BmString("Are you sure about removing the filter-chain <") << mCurrFilterChain->Key() << ">?").String(),
													 	 "Remove", "Cancel", NULL, B_WIDTH_AS_USUAL,
													 	 B_WARNING_ALERT);
					alert->SetShortcut( 1, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(BM_REMOVE_CHAIN), BMessenger( this)));
				} else {
					// second step, do it if user said ok:
					if (buttonPressed == 0) {
						TheFilterChainList->RemoveItemFromList( mCurrFilterChain.Get());
						mCurrFilterChain = NULL;
						NoticeChange();
					}
				}
				break;
			}
			case BM_CHAIN_FILTER: {
				BmChainedFilter* chainedFilter 
					= new BmChainedFilter( mCurrAvailableFilter->Key().String(), mCurrFilterChain.Get());
				mCurrFilterChain->AddItemToList( chainedFilter);
				NoticeChange();
				break;
			}
			case BM_UNCHAIN_FILTER: {
				mCurrFilterChain->RemoveItemFromList( mCurrChainedFilter.Get());
				NoticeChange();
				break;
			}
			case BM_EMPTY_CHAIN: {
				BmListModel* chain = dynamic_cast< BmListModel*>( mCurrFilterChain.Get());
				if (chain) {
					BmModelItemMap::const_iterator iter;
					while( (iter = chain->begin()) != chain->end())
						chain->RemoveItemFromList( iter->second.Get());
					NoticeChange();
				}
				break;
			}
			case BM_MOVE_UP_FILTER: {
				BmFilterChain* chain = dynamic_cast< BmFilterChain*>( mCurrFilterChain.Get());
				if (chain)
					chain->MoveUp( mCurrChainedFilter->Position());
				NoticeChange();
				break;
			}
			case BM_MOVE_DOWN_FILTER: {
				BmFilterChain* chain = dynamic_cast< BmFilterChain*>( mCurrFilterChain.Get());
				if (chain)
					chain->MoveDown( mCurrChainedFilter->Position());
				NoticeChange();
				break;
			}
			case BM_COMPLAIN_ABOUT_FIELD: {
				break;
			}
			default:
				inherited::MessageReceived( msg);
		}
	}
	catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("PrefsView_") << Name() << ":\n\t" << err.what());
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterChainView::UpdateState() {
	bool haveChain = (mCurrFilterChain != NULL);
	bool haveChained = (mCurrChainedFilter != NULL);
	bool haveAvailable = (mCurrAvailableFilter != NULL);

	mChainControl->SetEnabled( haveChain);
	mRemoveButton->SetEnabled( haveChain && mCurrFilterChain->Key()!=BM_DefaultItemLabel
										&& mCurrFilterChain->Key()!=BM_DefaultOutItemLabel);
	mMoveUpButton->SetEnabled( haveChain && haveChained);
	mMoveDownButton->SetEnabled( haveChain && haveChained);
	mAddFilterButton->SetEnabled( haveChain && haveAvailable);
	mRemoveFilterButton->SetEnabled( haveChain && haveChained);
	mEmptyChainButton->SetEnabled( haveChain);
	
	if (!haveChain) {
		mChainControl->SetTextSilently( "");
	} else {
		mChainControl->SetTextSilently( mCurrFilterChain->Name().String());
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmPrefsFilterChainView::CreateFilterChainListView( minimax minmax, int32 width, int32 height) {
	mFilterChainListView = BmFilterChainView::CreateInstance( minmax, width, height);
	mFilterChainListView->ClickSetsFocus( true);
	return mFilterChainListView->ContainerView();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmPrefsFilterChainView::CreateChainedFilterListView( minimax minmax, int32 width, int32 height) {
	mChainedFilterListView = BmChainedFilterView::CreateInstance( minmax, width, height);
	mChainedFilterListView->ClickSetsFocus( true);
	return mChainedFilterListView->ContainerView();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmPrefsFilterChainView::CreateAvailableFilterListView( minimax minmax, int32 width, int32 height) {
	mAvailableFilterListView = BmFilterView::CreateInstance( minmax, width, height);
	mAvailableFilterListView->ClickSetsFocus( true);
	return mAvailableFilterListView->ContainerView();
}