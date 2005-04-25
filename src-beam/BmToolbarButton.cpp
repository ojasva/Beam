/*
	BmToolbarButton.cpp
		$Id$
*/

#include <set>

#include <Bitmap.h>
#include <PopUpMenu.h>
#include <MenuItem.h>
#include "BmString.h"
#include <View.h>

#include "BubbleHelper.h"
#include "Colors.h"

#include "MWindow.h"

#include "BmBasics.h"
#include "BmBitmapHandle.h"
#include "BmPrefs.h"
#include "BmResources.h"
#include "BmToolbarButton.h"

enum {STATE_ON, STATE_OFF, STATE_DISABLED};
const float DIVW      = 6.0;
const float DIVH      = 2.0;
const float DIVLABELW = 10.0;
const float DIVLABELH = 5.0;
const float DIVICONW  = 10.0;
const float DIVICONH  = 6.0;
const float DIVLATCHW  = 10.0;
const float DIVLATCHH  = 8.0;
const float LATCHSZ  = 4.0;

static BPicture BmDummyPicture;


BmToolbarManager* BmToolbarManager::theInstance = NULL;

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbarManager* BmToolbarManager::Instance()
{
	if (!theInstance)
		theInstance = new BmToolbarManager;
	return theInstance;
};

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbarManager::BmToolbarManager()
	:	mToolbarLock( "ToolbarManager")
{
};

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarManager::UpdateAllToolbars() {
	mToolbarLock.Lock();
	BmToolbarSet::iterator iter;
	for( iter=mToolbarSet.begin(); iter != mToolbarSet.end(); ++iter) {
		BmToolbar* tb = (*iter);
		if (tb)
			tb->UpdateLayout();
	}
	mToolbarLock.Unlock();
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarManager::AddToolbar( BmToolbar* tb)
{
	mToolbarLock.Lock();
	mToolbarSet.insert( tb);
	mToolbarLock.Unlock();
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarManager::RemoveToolbar( BmToolbar* tb)
{
	mToolbarLock.Lock();
	mToolbarSet.insert( tb);
	mToolbarLock.Unlock();
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbar::BmToolbar(MView* kid)
	:	inherited( M_RAISED_BORDER, 1, NULL, kid)
{
	TheToolbarManager->AddToolbar( this);
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbar::~BmToolbar()
{
	TheToolbarManager->RemoveToolbar( this);
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbar::UpdateLayout() {
	if (LockLooper()) {
		BView* group = ChildAt(0);
		if (group) {
			int32 count = group->CountChildren();
			// Get maximum button size...
			float width=0, height=0;
			for( int32 c=0; c<count; ++c) {
				BmToolbarButton* tbb 
					= dynamic_cast<BmToolbarButton*>(group->ChildAt(c));
				if (tbb)
					BmToolbarButton::CalcMaxSize(width, height, tbb->Label().String(), 
														  tbb->NeedsLatch());
			}
			//...and layout all buttons according to this size:
			for( int32 c=0; c<count; ++c) {
				BmToolbarButton* tbb 
					= dynamic_cast<BmToolbarButton*>(group->ChildAt(c));
				if (tbb)
					tbb->CreateAllPictures(width, height);
			}
			MWindow* win = dynamic_cast<MWindow*>( Window());
			if (win)
				win->RecalcSize();
			for( int32 c=0; c<count; ++c)
				group->ChildAt(c)->Invalidate();
		}
		UnlockLooper();
	}
}



/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarSpace::Draw( BRect updateRect) {
	Space::Draw( updateRect);
	BmBitmapHandle* toolbarBackground 
		= TheResources->IconByName("Toolbar_Background");
	DrawBitmap( toolbarBackground->bitmap, Bounds());
}



/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbarButton::BmToolbarButton( const char *label, float width, float height, 
											 BMessage *message, BHandler *handler, 
											 const char* tipText, bool needsLatch,
											 const char* resourceName)
	:	inherited( minimax( width, height, -1 ,-1), 
					  &BmDummyPicture, &BmDummyPicture, message, handler)
	,	mHighlighted( false)
	,	mNeedsLatch( needsLatch)
	,	mUpdateVariationsFunc( NULL)
	,	mLabel( label)
	,	mResourceName( resourceName ? resourceName : label)
{
	TheBubbleHelper->SetHelp( this, tipText);
	SetViewColor( B_TRANSPARENT_COLOR);
	CreateAllPictures( width, height);
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BmToolbarButton::~BmToolbarButton() {
	TheBubbleHelper->SetHelp( this, NULL);
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::CreateAllPictures( float width, float height) {
	ct_mpm = minimax( width, height, -1, -1);
	BPicture* off_pic = 
		CreatePicture( STATE_OFF, width, height);
	BPicture* on_pic = 
		CreatePicture( STATE_ON, width, height);
	BPicture* dis_pic 
		= CreatePicture( STATE_DISABLED, width, height);
	SetEnabledOff( off_pic);
	SetEnabledOn( on_pic);
	SetDisabledOff( dis_pic);
	delete off_pic;
	delete on_pic;
	delete dis_pic;
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::AddActionVariation( const BmString label, BMessage* msg) {
	mVariations.push_back( BmVariation( label, msg));
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
BPicture* BmToolbarButton::CreatePicture( int32 mode, float width, 
														float height) {
	const char* label = mLabel.String();
	BmBitmapHandle* imageHandle 
		= TheResources->IconByName(BmString("Button_")<<mResourceName);
	BBitmap* image = imageHandle ? imageHandle->bitmap : NULL;
	// Calc icon/label positions
	BFont font( be_plain_font);
	bool showIcons = ThePrefs->GetBool( "ShowToolbarIcons", true);
	BmString labelMode = ThePrefs->GetString( "ShowToolbarLabel", "Bottom");
	float labelWidth = (label && labelMode != "Hide") ? font.StringWidth( label) : 0;
	float labelHeight = (label && labelMode != "Hide") ? TheResources->FontLineHeight( &font) : 0;
	float iconWidth  = (showIcons && image) ? image->Bounds().Width()  : 0;
	float iconHeight = (showIcons && image) ? image->Bounds().Height() : 0;
	float offset = (mode == STATE_ON) ? 1 : 0.0;

	BPoint posIcon( 0,0), posLabel( 0,0);
	if (showIcons && (labelMode == "Left")) {
		// Icon + Label/Left
		float d = (width-DIVLATCHW-DIVW-labelWidth-iconWidth)/2;
		posLabel = BPoint( d,(height-labelHeight)/2+offset);
		posIcon = BPoint( d+DIVW+labelWidth+(mNeedsLatch?DIVLATCHW:0),
								(height-iconHeight)/2-1+offset);
	} else if (showIcons && (labelMode == "Right")) {
		// Icon + Label/Right
		float d = (width-DIVLATCHW-DIVW-labelWidth-iconWidth)/2;
		posLabel = BPoint( d+DIVW+iconWidth,(height-labelHeight)/2+offset);
		posIcon = BPoint( d,(height-iconHeight)/2-1+offset);
	} else if (showIcons && (labelMode == "Top")) {
		// Icon + Label/top
		float d = (height-DIVH-labelHeight-iconHeight)/2+offset-2;
		posLabel = BPoint( (width-labelWidth)/2, d);
		posIcon = BPoint( (width-iconWidth)/2-1,d+DIVH+labelHeight);
	} else if (showIcons && (labelMode == "Bottom")) {
		// Icon + Label/bottom
		float d = (height-DIVH-labelHeight-iconHeight)/2+offset;
		posLabel = BPoint( (width-labelWidth)/2, d+DIVH+iconHeight);
		posIcon = BPoint( (width-iconWidth)/2-1,d);
	} else if (!showIcons && labelMode != "Hide") {
		// Label only
		posLabel = BPoint( (width-labelWidth)/2,(height-labelHeight)/2+offset);
	} else if (showIcons && labelMode == "Hide") {
		// Icon only
		posIcon = BPoint( (width-iconWidth)/2,(height-iconHeight)/2+offset);
	}
	
	if (mNeedsLatch) {
		if (labelMode == "Hide")
			mLatchRect.Set( posIcon.x+iconWidth+2, posIcon.y+iconHeight/2-LATCHSZ/2, 
								 width, height);
		else
			mLatchRect.Set( posLabel.x+labelWidth+2, posLabel.y+labelHeight/2-LATCHSZ/2, 
								 width, height);
	} else
		mLatchRect.Set( -1, -1, -1, -1);

	// Draw
	BRect rect(0,0,width-1,height-1);
	BView* view = new BView( rect, NULL, B_FOLLOW_NONE, 0);
	BBitmap* drawImage = new BBitmap( rect, B_RGBA32, true);
	drawImage->AddChild( view);
	drawImage->Lock();
	BPicture* picture = new BPicture();
	view->BeginPicture( picture);
	view->SetHighColor( ui_color( B_UI_PANEL_TEXT_COLOR));
	view->SetViewColor( B_TRANSPARENT_COLOR);
	view->SetLowColor( ui_color( B_UI_PANEL_BACKGROUND_COLOR));
	BmBitmapHandle* toolbarBackground 
		= TheResources->IconByName("Toolbar_Background");
	view->DrawBitmap( toolbarBackground->bitmap, view->Bounds());

	// Draw Border
	if (ThePrefs->GetBool( "ShowToolbarBorder", true)) {
		rect.InsetBy( 1, 1);
		view->SetLowColor( ui_color( B_UI_SHINE_COLOR));
		view->StrokeLine( BPoint( rect.left+2,  rect.top+1),    BPoint( rect.right-1, rect.top+1),    B_SOLID_LOW);	// top
		view->StrokeLine( BPoint( rect.right,   rect.top+2),    BPoint( rect.right,   rect.bottom-1), B_SOLID_LOW);	// right
		view->StrokeLine( BPoint( rect.right-1, rect.bottom),   BPoint( rect.left+2,  rect.bottom),   B_SOLID_LOW);	// bottom
		view->StrokeLine( BPoint( rect.left+1,  rect.bottom-1), BPoint( rect.left+1,  rect.top+2),    B_SOLID_LOW);	// left
		view->SetLowColor( BmWeakenColor(B_UI_SHADOW_COLOR, BeShadowMod));
		view->StrokeLine( BPoint( rect.left+1,  rect.top),      BPoint( rect.right-2, rect.top),      B_SOLID_LOW);	// top
		view->StrokeLine( BPoint( rect.right-1, rect.top+1),    BPoint( rect.right-1, rect.bottom-2), B_SOLID_LOW);	// right
		view->StrokeLine( BPoint( rect.right-2, rect.bottom-1), BPoint( rect.left+1,  rect.bottom-1), B_SOLID_LOW);	// bottom
		view->StrokeLine( BPoint( rect.left,    rect.bottom-2), BPoint( rect.left,    rect.top+1),    B_SOLID_LOW);	// left
		if (ThePrefs->GetBool( "DebugToolbar", false)) {
			view->SetLowColor( 255,0,0);
			view->StrokeRect(BRect( 0,0,width-1,height-1), B_SOLID_LOW);
		}
	}

	// Draw Icon
	if (showIcons && image) {
		view->SetDrawingMode( B_OP_ALPHA);
		view->SetBlendingMode( B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		if (mode == STATE_OFF) {
			view->DrawBitmap( image, posIcon+BPoint( 1,1));
			view->SetLowColor( ui_color( B_UI_PANEL_BACKGROUND_COLOR));
		}
		view->DrawBitmap( image, posIcon);
		view->SetDrawingMode(B_OP_COPY);
#ifdef DEBUG
		if (ThePrefs->GetBool( "DebugToolbar", false)) {
			view->SetLowColor(0,0,255);
			view->StrokeRect( BRect(posIcon, posIcon+BPoint(iconWidth, iconHeight)), B_SOLID_LOW);
		}
#endif
	}

	// Draw Label
	if (labelMode != "Hide" && label) {
		font_height fh;
		be_plain_font->GetHeight( &fh);
		view->SetFont( &font);
		view->SetDrawingMode(B_OP_OVER);
		view->DrawString( label, posLabel+BPoint(0,fh.ascent+1));

#ifdef DEBUG
		if (ThePrefs->GetBool( "DebugToolbar", false)) {
			view->SetLowColor(0,255,0);
			view->StrokeRect( BRect(posLabel, posLabel+BPoint(labelWidth, labelHeight)), B_SOLID_LOW);
			view->SetLowColor(0,255,0);
			view->StrokeRect( BRect(posLabel+BPoint(0,0), posLabel+BPoint(labelWidth, labelHeight)),    B_SOLID_LOW);
		}
#endif
	}

	// draw latch
	if (mNeedsLatch) {
		float x_offs = mLatchRect.left;
		float y_offs = mLatchRect.top+1;
		view->FillTriangle( BPoint( x_offs, y_offs), 
								  BPoint( x_offs+LATCHSZ*2, y_offs),
								  BPoint( x_offs+LATCHSZ, y_offs+LATCHSZ));
	}

	if (mode == STATE_DISABLED) {
		// blend complete picture into background:
		view->SetDrawingMode( B_OP_BLEND);
		view->DrawBitmap( toolbarBackground->bitmap, view->Bounds());
		view->DrawBitmap( toolbarBackground->bitmap, view->Bounds());
	}

	view->EndPicture();
	drawImage->Unlock();
	delete drawImage;
	return picture;
}

/*------------------------------------------------------------------------------*\
	SetUpdateVariationsFunc( updFunc)
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::SetUpdateVariationsFunc( BmUpdateVariationsFunc* updFunc)
{
	mUpdateVariationsFunc = updFunc;
}

/*------------------------------------------------------------------------------*\
	MouseDown( point)
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::MouseDown( BPoint point) {
	if (IsEnabled() && mLatchRect.Contains( point))
		ShowMenu( BPoint( 0.0, mLatchRect.top+3+LATCHSZ));
	else
		inherited::MouseDown( point); 
}

/*------------------------------------------------------------------------------*\
	ShowMenu()
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::ShowMenu( BPoint point) {
	BPopUpMenu* theMenu = new BPopUpMenu( "ButtonMenu", false, false);
	theMenu->SetFont( be_plain_font);
	
	if (mUpdateVariationsFunc) {
		mVariations.clear();
		mUpdateVariationsFunc( this);
	}

	BMenuItem* item = NULL;
	for( uint32 i=0; i<mVariations.size(); ++i) {
		item = new BMenuItem( mVariations[i].label.String(), 
								 	 new BMessage( *mVariations[i].msg));
		item->SetTarget( Messenger());
		theMenu->AddItem( item);
	}

   ConvertToScreen(&point);
	BRect openRect;
	openRect.top = point.y - 15;
	openRect.bottom = point.y + 10;
	openRect.left = point.x - 10;
	openRect.right = point.x + 105;
	theMenu->SetAsyncAutoDestruct( true);
  	theMenu->Go( point, true, false, openRect, true);
}

/*------------------------------------------------------------------------------*\
	( )
		-	
\*------------------------------------------------------------------------------*/
void BmToolbarButton::CalcMaxSize( float& width, float& height, 
											  const char* label, bool needsLatch) {
	BFont font( be_plain_font);
	float w, h;
	BmBitmapHandle* imageHandle 
		= TheResources->IconByName( BmString("Button_")<<label);
	BBitmap* image = imageHandle ? imageHandle->bitmap : NULL;
	bool showIcons = ThePrefs->GetBool( "ShowToolbarIcons", true);
	BmString labelMode   = ThePrefs->GetString( "ShowToolbarLabel", "Bottom");
	float labelWidth  = label ? font.StringWidth( label) : 0;
	float labelHeight = label ? TheResources->FontLineHeight( &font) : 0;
	float iconWidth   = (showIcons && image) ? image->Bounds().Width()  : 0;
	float iconHeight  = (showIcons && image) ? image->Bounds().Height() : 0;

	if (showIcons && (labelMode == "Left" || labelMode == "Right")) {
		// Icon + Label: horizontal
		w = labelWidth+iconWidth+DIVW+2*DIVICONW + (needsLatch?DIVLATCHW:0);
		h = MAX( labelHeight+2*DIVLABELH, iconHeight+2*DIVICONH);
	} else if (showIcons && (labelMode == "Top" || labelMode == "Bottom")) {
		// Icon + Label: vertical
		w = MAX( labelWidth+2*DIVLABELW, iconWidth+2*DIVICONW) + (needsLatch?DIVLATCHW:0);
		h = labelHeight+iconHeight+DIVH+2*DIVICONH;
	} else if (!showIcons && labelMode != "Hide") {
		// Label only
		w = labelWidth+2*DIVLABELW + (needsLatch?DIVLATCHW:0);
		h = labelHeight+2*DIVLABELH;
	} else /* if (showIcons && labelMode == "Hide") */ {
		// Icon only
		w = iconWidth+2*DIVICONW + (needsLatch?DIVLATCHW:0);
		h = iconHeight+2*DIVICONH;
	}

	width  = MAX( width,  w);
	height = MAX( height, h - (ThePrefs->GetBool( "ToolbarBorder", true) ? 0 : 6));
}

