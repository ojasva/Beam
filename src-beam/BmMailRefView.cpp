/*
	BmMailRefView.cpp
		$Id$
*/

#include <Window.h>

#include "BmBasics.h"
#include "BmLogHandler.h"
#include "BmMailFolder.h"
#include "BmMailRef.h"
#include "BmMailRefList.h"
#include "BmMailRefView.h"
#include "BmMailView.h"
#include "BmMsgTypes.h"
#include "BmPrefs.h"
#include "BmResources.h"
#include "BmStorageUtil.h"
#include "BmUtil.h"

const int16 BmMailRefItem::nFirstTextCol = 3;

/********************************************************************************\
	BmMailRefItem
\********************************************************************************/


/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailRefItem::BmMailRefItem( BString key, BmListModelItem* _item)
	:	inherited( key, _item)
{
	BmMailRef* ref = dynamic_cast<BmMailRef*>( _item);

	BString st = BString("Mail_") << ref->Status();
	BBitmap* icon = TheResources->IconByName(st);
	SetColumnContent( 0, icon, 2.0, false);

	if (ref->HasAttachments()) {
		icon = TheResources->IconByName("Attachment");
		SetColumnContent( 1, icon, 2.0, false);
	}
	
	BString priority = BString("Priority_") << ref->Priority();
	if ((icon = TheResources->IconByName(priority))) {
		SetColumnContent( 2, icon, 2.0, false);
	}

	BmListColumn cols[] = {
		{ ref->From().String(),						false },
		{ ref->Subject().String(),					false },
		{ ref->WhenString().String(),				false },
		{ ref->SizeString().String(),				true  },
		{ ref->Cc().String(),						false },
		{ ref->Account().String(),					false },
		{ ref->To().String(),						false },
		{ ref->ReplyTo().String(),					false },
		{ ref->Name().String(),						false },
		{ ref->CreatedString().String(),			false },
		{ ref->TrackerName(),						false },
		{ ref->Status().String(),					false },
		{ ref->HasAttachments() ? "*" : "",		false },
		{ ref->Priority().String(),				false },
		{ NULL, false }
	};
	SetTextCols( nFirstTextCol, cols, !ThePrefs->StripedListView());
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailRefItem::~BmMailRefItem() { 
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
const int32 BmMailRefItem::GetNumValueForColumn( int32 column_index) const {
	BmMailRef* ref = ModelItem();
	if (column_index == 0 || column_index == 14) {
		// status
		BString st = ref->Status();
		return st == "New" 			? 0 :
				 st == "Read" 			? 1 :
				 st == "Forwarded" 	? 2 :
				 st == "Replied" 		? 3 :
				 st == "Pending" 		? 4 :
				 st == "Sent" 			? 5 : 99;
	} else if (column_index == 1 || column_index == 15) {
		// attachment
		return ref->HasAttachments() ? 0 : 1;	
							// show mails with attachment at top (with sortmode 'ascending')
	} else if (column_index == 2 || column_index == 16) {
		// priority
		int16 prio = atol( ref->Priority().String());
		return (prio>=1 && prio<=5) ? prio : 3;
							// illdefined priority means medium priority (=3)
	} else if (column_index == 6) {
		// size
		return ref->Size();
	} else {
		return 0;
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
const time_t BmMailRefItem::GetDateValueForColumn( int32 column_index) const {
	const BmMailRef* ref = ModelItem();
	if (column_index == 5)
		return ref->When();
	else if (column_index == 12)
		return ref->Created();
	else
		return 0;
}



/********************************************************************************\
	BmMailRefView
\********************************************************************************/


BmMailRefView* BmMailRefView::theInstance = NULL;

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailRefView* BmMailRefView::CreateInstance( minimax minmax, int32 width, int32 height) {
	if (theInstance)
		return theInstance;
	else 
		return theInstance = new BmMailRefView( minmax, width, height);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailRefView::BmMailRefView( minimax minmax, int32 width, int32 height)
	:	inherited( minmax, BRect(0,0,width-1,height-1), "Beam_MailRefView", B_MULTIPLE_SELECTION_LIST, 
					  false, true, true, true)
	,	mCurrFolder( NULL)
{
	int32 flags = 0;
	SetViewColor( B_TRANSPARENT_COLOR);
	if (ThePrefs->StripedListView())
		SetStripedBackground( true);
	else 
		flags |= CLV_TELL_ITEMS_WIDTH;
	Initialize( BRect(0,0,width-1,height-1), B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE,
					B_FOLLOW_ALL, true, true, true, B_FANCY_BORDER);

	AddColumn( new CLVColumn( "", 18.0, CLV_SORT_KEYABLE | CLV_NOT_RESIZABLE | CLV_COLDATA_NUMBER, 
									  18.0, "Status [Icon]"));
	AddColumn( new CLVColumn( "A", 18.0, CLV_SORT_KEYABLE | CLV_NOT_RESIZABLE | CLV_COLDATA_NUMBER, 
									  18.0, "(A)ttachments [Icon]"));
	AddColumn( new CLVColumn( "P", 18.0, CLV_SORT_KEYABLE | CLV_NOT_RESIZABLE | CLV_COLDATA_NUMBER, 
									  18.0, "(P)riority [Icon]"));
	AddColumn( new CLVColumn( "From", 200.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Subject", 200.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Date", 100.0, CLV_SORT_KEYABLE | CLV_COLDATA_DATE | flags, 20.0));
	AddColumn( new CLVColumn( "Size", 50.0, CLV_SORT_KEYABLE | CLV_COLDATA_NUMBER | CLV_RIGHT_JUSTIFIED | flags, 20.0));
	AddColumn( new CLVColumn( "Cc", 100.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Account", 100.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "To", 100.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Reply-To", 150.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Name", 150.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "Created", 100.0, CLV_SORT_KEYABLE | CLV_COLDATA_DATE | flags, 20.0));
	AddColumn( new CLVColumn( "Tracker-Name", 150.0, CLV_SORT_KEYABLE | flags, 20.0));
	AddColumn( new CLVColumn( "S", 100.0, CLV_SORT_KEYABLE, 40.0, "(S)tatus [Text]"));
	AddColumn( new CLVColumn( "A", 100.0, CLV_SORT_KEYABLE | CLV_COLDATA_NUMBER, 18.0, "(A)ttachments [Text]"));
	AddColumn( new CLVColumn( "P", 100.0, CLV_SORT_KEYABLE | CLV_COLDATA_NUMBER, 18.0, "(P)riority [Text]"));
	SetSortFunction( CLVEasyItem::CompareItems);
	SetSortKey( 3);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailRefView::~BmMailRefView() { 
	theInstance = NULL;
}

/*------------------------------------------------------------------------------*\
	CreateContainer()
		-	
\*------------------------------------------------------------------------------*/
CLVContainerView* BmMailRefView::CreateContainer( bool horizontal, bool vertical, 
												  				  bool scroll_view_corner, 
												  				  border_style border, 
																  uint32 ResizingMode, 
																  uint32 flags) 
{
	return new BmCLVContainerView( fMinMax, this, ResizingMode, flags, horizontal, 
											 vertical, scroll_view_corner, border, mShowCaption,
											 mShowBusyView, 
											 be_plain_font->StringWidth(" 99999 messages "));
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmMailRefView::CreateListViewItem( BmListModelItem* item, 
																	BMessage* archive) {
	return new BmMailRefItem( item->Key(), item);
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmMailRefView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			case B_TRASH_TARGET: {
				if (msg->IsReply()) {
					const BMessage* origMsg = msg->Previous();
					if (origMsg) {
						int32 count;
						type_code code;
						origMsg->GetInfo( "refs", &code, &count);
						entry_ref* refs = new entry_ref [count];
						int i=0;
						while( origMsg->FindRef( "refs", i, &refs[i]) == B_OK)
							++i;
						MoveToTrash( refs, i);
						delete [] refs;
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
		BM_SHOWERR( BString("MailRefView: ") << err.what());
	}
}

/*------------------------------------------------------------------------------*\
	KeyDown()
		-	
\*------------------------------------------------------------------------------*/
void BmMailRefView::KeyDown(const char *bytes, int32 numBytes) { 
	if ( numBytes == 1 ) {
		switch( bytes[0]) {
			case B_PAGE_UP:
			case B_PAGE_DOWN:
			case B_UP_ARROW:
			case B_DOWN_ARROW:
			case B_LEFT_ARROW:
			case B_RIGHT_ARROW: {
				int32 mods = Window()->CurrentMessage()->FindInt32("modifiers");
				if (mods & (B_CONTROL_KEY)) {
					// remove modifiers so we don't ping-pong endlessly:
					Window()->CurrentMessage()->ReplaceInt32("modifiers", 0);
					if (mPartnerMailView)
						mPartnerMailView->KeyDown( bytes, numBytes);
				} else
					inherited::KeyDown( bytes, numBytes);
				break;
			}
			default:
				inherited::KeyDown( bytes, numBytes);
				break;
		}
	}
}

/*------------------------------------------------------------------------------*\
	InitiateDrag()
		-	
\*------------------------------------------------------------------------------*/
bool BmMailRefView::InitiateDrag( BPoint where, int32 index, bool wasSelected) {
	if (!wasSelected)
		return false;
	BMessage dragMsg( BM_MAIL_DRAG);
	dragMsg.AddString( "be:types", "text/x-email");
	dragMsg.AddString( "be:type_descriptions", "E-mail");
	dragMsg.AddInt32( "be:actions", B_MOVE_TARGET);
	dragMsg.AddInt32( "be:actions", B_TRASH_TARGET);
	BmMailRefItem* refItem = dynamic_cast<BmMailRefItem*>(ItemAt( index));
	BmMailRef* ref = dynamic_cast<BmMailRef*>(refItem->ModelItem());
	dragMsg.AddString( "be:clip_name", ref->TrackerName());
	dragMsg.AddString( "be:originator", "Beam");
	int32 currIdx;
	// we count the number of selected items:
	int32 selCount;
	for( selCount=0; (currIdx=CurrentSelection( selCount))>=0; ++selCount)
		;
	BFont font;
	GetFont( &font);
	float lineHeight = MAX(TheResources->FontLineHeight( &font),20.0);
	float baselineOffset = TheResources->FontBaselineOffset( &font);
	BRect dragRect( 0, 0, 200-1, MIN(selCount,4.0)*lineHeight-1);
	BView* dummyView = new BView( dragRect, NULL, B_FOLLOW_NONE, 0);
	BBitmap* dragImage = new BBitmap( dragRect, B_RGBA32, true);
	dragImage->AddChild( dummyView);
	dragImage->Lock();
	dummyView->SetHighColor( B_TRANSPARENT_COLOR);
	dummyView->FillRect( dragRect);
	dummyView->SetDrawingMode( B_OP_ALPHA);
	dummyView->SetHighColor( 0, 0, 0, 128);
	dummyView->SetBlendingMode( B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
	for( int32 i=0; (currIdx=CurrentSelection( i))>=0; ++i) {
		// now we add all selected items to drag-image and to drag-msg:
		refItem = dynamic_cast<BmMailRefItem*>(ItemAt( currIdx));
		ref = dynamic_cast<BmMailRef*>(refItem->ModelItem());
		dragMsg.AddRef( "refs", ref->EntryRefPtr());
		if (i<3) {
			// add only the first three selections to drag-image:
			const BBitmap* icon = refItem->GetColumnContentBitmap( 0);
			if (icon) {
				dummyView->DrawBitmapAsync( icon, BPoint(0,i*lineHeight));
			}
			dummyView->DrawString( ref->Subject().String(), 
										  BPoint( 20.0, i*lineHeight+baselineOffset));
		} else if (i==3) {
			// add an indicator that more items are being dragged than shown:
			BString indicator = BString("(...and ") << selCount-3 << " more items)";
			dummyView->DrawString( indicator.String(), 
										  BPoint( 20.0, i*lineHeight+baselineOffset));
		}
	}
	dragImage->Unlock();
	DragMessage( &dragMsg, dragImage, B_OP_ALPHA, BPoint( 10.0, 10.0));
	return true;
}

/*------------------------------------------------------------------------------*\
	AcceptsDropOf( msg)
		-	
\*------------------------------------------------------------------------------*/
bool BmMailRefView::AcceptsDropOf( const BMessage* msg) {
	if (mCurrFolder && msg && msg->what == B_SIMPLE_DATA) {
		entry_ref eref;
		bool containsMails = false;
		for( int32 i=0; msg->FindRef( "refs", i, &eref) == B_OK; ++i) {
			if (CheckMimeType( &eref, "text/x-email"))
				containsMails = true;
		}
		return containsMails;
	} else
		return false;
}

/*------------------------------------------------------------------------------*\
	HandleDrop( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmMailRefView::HandleDrop( const BMessage* msg) {
	if (mCurrFolder && msg && msg->what == B_SIMPLE_DATA) {
		BList refList;
		entry_ref eref;
		for( int i=0; msg->FindRef( "refs", i, &eref)==B_OK; ++i) {
			refList.AddItem( new entry_ref( eref));
		}
		mCurrFolder->MoveMailsHere( refList);
		entry_ref* ref;
		while( (ref = static_cast<entry_ref*>( refList.RemoveItem( (int32)0))))
			delete ref;
	}
	inherited::HandleDrop( msg);
}

/*------------------------------------------------------------------------------*\
	ShowFolder()
		-	
\*------------------------------------------------------------------------------*/
void BmMailRefView::ShowFolder( BmMailFolder* folder) {
	try {
		StopJob();
		BmMailRefList* refList = folder ? folder->MailRefList() : NULL;
		if (mPartnerMailView)
			mPartnerMailView->ShowMail( NULL);
		if (refList)
			StartJob( refList, true);
		mCurrFolder = folder;
	}
	catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BString("MailRefView: ") << err.what());
	}
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BString BmMailRefView::StateInfoBasename()	{ 
	return "MailRefView";
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BMessage* BmMailRefView::DefaultLayout()		{ 
	return ThePrefs->MailRefLayout(); 
}

/*------------------------------------------------------------------------------*\
	SelectionChanged()
		-	
\*------------------------------------------------------------------------------*/
void BmMailRefView::SelectionChanged( void) {
	int32 selection = CurrentSelection();
	if (selection >= 0) {
		BmMailRefItem* refItem;
		refItem = dynamic_cast<BmMailRefItem*>(ItemAt( selection));
		if (refItem) {
			BmMailRef* ref = dynamic_cast<BmMailRef*>(refItem->ModelItem());
			if (ref && mPartnerMailView)
				mPartnerMailView->ShowMail( ref);
		}
	} else
		if (mPartnerMailView)
			mPartnerMailView->ShowMail( NULL);
}