/*
	BmPrefsFilterView.cpp
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
#include <liblayout/MPopup.h>
#include <liblayout/MStringView.h>
#include <liblayout/MTabView.h>
#include <liblayout/Space.h>
#include <liblayout/VGroup.h>

#include "BubbleHelper.h"
#include "Colors.h"
#include "ColumnListView.h"
#include "CLVEasyItem.h"

#include "BmCheckControl.h"
#include "BmGuiUtil.h"
#include "BmLogHandler.h"
#include "BmMultiLineTextControl.h"
#include "BmPrefs.h"
#include "BmPrefsFilterView.h"
#include "BmTextControl.h"
#include "BmUtil.h"

/********************************************************************************\
	BmFilterItem
\********************************************************************************/

enum Columns {
	COL_KEY = 0,
	COL_DEFAULT,
	COL_CONTENT
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterItem::BmFilterItem( const BmString& key, BmListModelItem* _item)
	:	inherited( key, _item, false)
{
	UpdateView( UPD_ALL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterItem::~BmFilterItem() { 
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmFilterItem::UpdateView( BmUpdFlags flags) {
	inherited::UpdateView( flags);
	BmFilter* filter = ModelItem();
	if (flags & UPD_ALL) {
		BmString beautifiedContent( filter->Content());
		beautifiedContent.ReplaceAll( "\n", "\\n");
		beautifiedContent.ReplaceAll( "\t", "\\t");
		
		BmListColumn cols[] = {
			{ filter->Key().String(),						false },
			{ filter->MarkedAsDefault() ? "*" : "",	false },
			{ beautifiedContent.String(),					false },
			{ NULL, false }
		};
		SetTextCols( 0, cols);
	}
}



/********************************************************************************\
	BmFilterView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterView* BmFilterView::CreateInstance( minimax minmax, int32 width, int32 height) {
	return new BmFilterView( minmax, width, height);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterView::BmFilterView( minimax minmax, int32 width, int32 height)
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

	AddColumn( new CLVColumn( "Name", 120.0, CLV_SORT_KEYABLE|flags, 50.0));
	AddColumn( new CLVColumn( "D", 20.0, CLV_SORT_KEYABLE|flags, 20.0, "(D)efault?"));
	AddColumn( new CLVColumn( "Content", 400.0, flags, 40.0));

	SetSortFunction( CLVEasyItem::CompareItems);
	SetSortKey( COL_KEY);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFilterView::~BmFilterView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmFilterView::CreateListViewItem( BmListModelItem* item,
																	  BMessage*) {
	return new BmFilterItem( item->Key(), item);
}

/*------------------------------------------------------------------------------*\
	CreateContainer()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmFilterView::CreateContainer( bool horizontal, bool vertical, 
												  				  bool scroll_view_corner, 
												  				  border_style border, 
																  uint32 ResizingMode, 
																  uint32 flags) 
{
	return new BmCLVContainerView( fMinMax, this, ResizingMode, flags, horizontal, 
											 vertical, scroll_view_corner, border, mShowCaption,
											 mShowBusyView, 
											 be_plain_font->StringWidth(" 99 filters "));
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmFilterView::AddModelItem( BmListModelItem* item) {
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
void BmFilterView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			default:
				inherited::MessageReceived( msg);
		}
	} catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("FilterView:\n\t") << err.what());
	}
}



/********************************************************************************\
	BmPrefsFilterView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsFilterView::BmPrefsFilterView( BmFilterList* filterList, bool outbound) 
	:	inherited( (BmString("Filters ")<<(outbound?"(outbound)":"(inbound)")).String())
	,	mFilterList( filterList)
	,	mOutbound( outbound)
{
	MTabView* tabView = NULL;
	MView* view = 
		new VGroup(
			CreateFilterListView( minimax(400,60,1E5,1E5,1), 400, 100),
			new HGroup(
				mAddButton = new MButton("Add Filter", new BMessage(BM_ADD_FILTER), this),
				mRemoveButton = new MButton("Remove Filter", new BMessage( BM_REMOVE_FILTER), this),
				0
			),
			new Space( minimax(0,10,0,10)),
			new MBorder( M_LABELED_BORDER, 10, (char*)"Filter Info",
				new VGroup(
					new HGroup( 
						mFilterControl = new BmTextControl( "Filter name:"),
						new Space(),
						mIsDefaultControl = new BmCheckControl( "Use this as default filter", 
																			 new BMessage(BM_IS_DEFAULT_CHANGED), 
																			 this),
						new Space(),
						mTestButton = new MButton( "Check the SIEVE-script", new BMessage( BM_TEST_FILTER), this, minimax(-1,-1,-1,-1)),
						0
					),
					new Space( minimax(0,5,0,5)),
					tabView = new MTabView(),
					0
				)
			),
			0
		);

	tabView->Add(
		new MTab( 
			new Space(minimax(0,0,1E5,1E5,0.5)), 
			(char*)"Graphical View"
		)
	);
	tabView->Add(
		new MTab( 
			new VGroup( 
				mContentControl = new BmMultiLineTextControl( "SIEVE-script:", false, 15), 
				new Space( minimax(0,2,0,2)),
				0
			),
			(char*)"Plain View"
		)
	);

	mGroupView->AddChild( dynamic_cast<BView*>(view));

	float divider = mFilterControl->Divider();
	divider = MAX( divider, mContentControl->Divider());
	mFilterControl->SetDivider( divider);
	mContentControl->SetDivider( divider);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsFilterView::~BmPrefsFilterView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::Initialize() {
	inherited::Initialize();

	TheBubbleHelper.SetHelp( mFilterListView, "This listview shows every filter you have defined.");
	TheBubbleHelper.SetHelp( mFilterControl, "Here you can enter a name for this filter.\nThis name is used to identify this filter in Beam.");
	if (mOutbound)
		TheBubbleHelper.SetHelp( mIsDefaultControl, "Checking this makes Beam use this filter \nas the default filter for outbound mail.");
	else
		TheBubbleHelper.SetHelp( mIsDefaultControl, "Checking this makes Beam use this filter \nas the default filter for inbound mail.");
	TheBubbleHelper.SetHelp( mContentControl, "Here you can enter the content of this filter (a SIEVE script).");
	TheBubbleHelper.SetHelp( mTestButton, "Here you can check the syntax of the SIEVE-script.");

	mFilterControl->SetTarget( this);
	mContentControl->SetTarget( this);
	mIsDefaultControl->SetTarget( this);

	mFilterListView->SetSelectionMessage( new BMessage( BM_SELECTION_CHANGED));
	mFilterListView->SetTarget( this);
	mFilterListView->StartJob( mFilterList);
	ShowFilter( -1);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::Activated() {
	inherited::Activated();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::WriteStateInfo() {
	mFilterListView->WriteStateInfo();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::SaveData() {
	mFilterList->Store();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::UndoChanges() {
	mFilterList->Cleanup();
	mFilterList->StartJobInThisThread();
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsFilterView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			case BM_SELECTION_CHANGED: {
				int32 index = mFilterListView->CurrentSelection( 0);
				ShowFilter( index);
				break;
			}
			case BM_MULTILINE_TEXTFIELD_MODIFIED: {
				if (mCurrFilter) {
					BView* srcView = NULL;
					msg->FindPointer( "source", (void**)&srcView);
					BmMultiLineTextControl* source = dynamic_cast<BmMultiLineTextControl*>( srcView);
					if ( source == mContentControl)
						mCurrFilter->Content( mContentControl->Text());
				}
				break;
			}
			case BM_IS_DEFAULT_CHANGED: {
				if (mCurrFilter) {
					bool val = mIsDefaultControl->Value();
					if (val) {
						if (mOutbound)
							TheOutboundFilterList->SetDefaultFilter( mCurrFilter->Key());
						else
							TheInboundFilterList->SetDefaultFilter( mCurrFilter->Key());
					} else
						mCurrFilter->MarkedAsDefault( false);
				}
				break;
			}
			case BM_TEXTFIELD_MODIFIED: {
				BView* srcView = NULL;
				msg->FindPointer( "source", (void**)&srcView);
				BmTextControl* source = dynamic_cast<BmTextControl*>( srcView);
				if ( mCurrFilter && source == mFilterControl)
					mFilterList->RenameItem( mCurrFilter->Name(), mFilterControl->Text());
				break;
			}
			case BM_TEST_FILTER: {
				if (mCurrFilter) {
					if (!mCurrFilter->CompileScript()) {
						BmString errString = mCurrFilter->LastErr() + "\n\n" 
													<< "Error: " 
													<< sieve_strerror(mCurrFilter->LastErrVal()) 
													<< "\n\n"
													<< mCurrFilter->LastSieveErr();
						BAlert* alert = new BAlert( "Filter-Test", errString.String(),
													 		 "OK", NULL, NULL, B_WIDTH_AS_USUAL,
													 		 B_INFO_ALERT);
						alert->SetShortcut( 0, B_ESCAPE);
						alert->Go();
					}
				}
				break;
			}
			case BM_ADD_FILTER: {
				BmString key( "new filter");
				for( int32 i=1; mFilterList->FindItemByKey( key); ++i) {
					key = BmString("new filter_")<<i;
				}
				mFilterList->AddItemToList( new BmFilter( key.String(), mFilterList));
				mFilterControl->MakeFocus( true);
				mFilterControl->TextView()->SelectAll();
				break;
			}
			case BM_REMOVE_FILTER: {
				int32 buttonPressed;
				if (msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, ask user about it:
					BAlert* alert = new BAlert( "Remove Filter", 
														 (BmString("Are you sure about removing the filter <") << mCurrFilter->Name() << ">?").String(),
													 	 "Remove", "Cancel", NULL, B_WIDTH_AS_USUAL,
													 	 B_WARNING_ALERT);
					alert->SetShortcut( 1, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(BM_REMOVE_FILTER), BMessenger( this)));
				} else {
					// second step, do it if user said ok:
					if (buttonPressed == 0) {
						mFilterList->RemoveItemFromList( mCurrFilter.Get());
						mCurrFilter = NULL;
					}
				}
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
void BmPrefsFilterView::ShowFilter( int32 selection) {
	bool enabled = (selection != -1);
	mFilterControl->SetEnabled( enabled);
	mIsDefaultControl->SetEnabled( enabled);
	mContentControl->SetEnabled( enabled);
	
	if (selection == -1) {
		mCurrFilter = NULL;
		mFilterControl->SetTextSilently( "");
		mIsDefaultControl->SetValue( 0);
		mContentControl->SetTextSilently( "");
		mTestButton->SetEnabled( false);
	} else {
		BmFilterItem* filterItem = dynamic_cast<BmFilterItem*>(mFilterListView->ItemAt( selection));
		if (filterItem) {
			mCurrFilter = filterItem->ModelItem();
			if (mCurrFilter) {
				mFilterControl->SetTextSilently( mCurrFilter->Name().String());
				mContentControl->SetTextSilently( mCurrFilter->Content().String());
				mIsDefaultControl->SetValue( mCurrFilter->MarkedAsDefault());
				mTestButton->SetEnabled( true);
			}
		} else
			mCurrFilter = NULL;
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmPrefsFilterView::CreateFilterListView( minimax minmax, int32 width, int32 height) {
	mFilterListView = BmFilterView::CreateInstance( minmax, width, height);
	mFilterListView->ClickSetsFocus( true);
	return mFilterListView->ContainerView();
}
