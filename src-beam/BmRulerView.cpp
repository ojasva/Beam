/*
	BmRulerView.cpp
		$Id$
*/
#include "Colors.h"

#include "BmRulerView.h"
#include "BmMailView.h"
#include "BmPrefs.h"

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRulerView::BmRulerView( const BFont& font)
	:	inherited( BRect( 0, 0, 0, 19),
					  "RulerView", B_FOLLOW_NONE, B_WILL_DRAW)
	,	mMailViewFont( font)
	,	mIndicatorPos( ThePrefs->GetInt( "MaxLineLen"))
	,	mIndicatorGrabbed( false)
	,	mSingleCharWidth( mMailViewFont.StringWidth( "W"))
{
	SetViewColor( BeInactiveControlGrey);
	BFont font( be_plain_font);
	font.SetSize( 8);
	SetFont( &font);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRulerView::~BmRulerView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmRulerView::Draw( BRect bounds) {
	inherited::Draw( bounds);
	
	BRect r = Bounds();
	
	float width = r.Width();
	float step = mSingleCharWidth;
	
	// draw ruler lines:
	for( float x=0; x<width; x+=step) {
		if (static_cast<int>(x)%10 == 0)
			StrokeLine( BPoint(nXOffset+x, r.bottom-10), BPoint(nXOffset+x, r.bottom));
		else if (static_cast<int>(x)%5 == 0)
			StrokeLine( BPoint(nXOffset+x, r.bottom-7), BPoint(nXOffset+x, r.bottom));
		else
			StrokeLine( BPoint(nXOffset+x, r.bottom-5), BPoint(nXOffset+x, r.bottom));
	}
	
	SetLowColor( ViewColor());
	// draw ruler numbers:
	int num=0;
	for( float x=0; x<width; x+=step*10) {
		BString numStr = BString("") << num;
		num += 10;
		float w = StringWidth( numStr.String());
		DrawString( numStr.String(), BPoint( nXOffset+1+x-w/2, 8));
	}

	// draw right-margin indicator:
	float xPos = nXOffset+mIndicatorPos*mSingleCharWidth;
	float yPos = r.bottom-10;
	if (!mIndicatorGrabbed)
		SetHighColor( LightMetallicBlue);
	else
		SetHighColor( MedMetallicBlue);
	FillTriangle( BPoint( xPos-5, yPos),
					  BPoint( xPos+5, yPos),
					  BPoint( xPos, yPos+10));
}

/*------------------------------------------------------------------------------*\
	MouseDown( point)
		-	
\*------------------------------------------------------------------------------*/
void BmRulerView::MouseDown( BPoint point) {
	inherited::MouseDown( point); 
	if (Parent())
		Parent()->MakeFocus( true);
	BPoint mousePos;
	uint32 buttons;
	GetMouse( &mousePos, &buttons);
	if (buttons == B_PRIMARY_MOUSE_BUTTON) {
		if (!mIndicatorGrabbed) {
			mIndicatorGrabbed = true;
			SetIndicatorPos( point.x);
			SetMouseEventMask( B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
		}
	}
}

/*------------------------------------------------------------------------------*\
	MouseMoved( point, transit, msg)
		-	
\*------------------------------------------------------------------------------*/
void BmRulerView::MouseMoved( BPoint point, uint32 transit, const BMessage *msg) {
	inherited::MouseMoved( point, transit, msg);
//	if (transit == B_INSIDE_VIEW || transit == B_ENTERED_VIEW) {
		if (mIndicatorGrabbed)
			SetIndicatorPos( point.x);
//	}
}

/*------------------------------------------------------------------------------*\
	MouseUp( point)
		-	
\*------------------------------------------------------------------------------*/
void BmRulerView::MouseUp( BPoint point) {
	inherited::MouseUp( point); 
	SetMouseEventMask( 0);
	if (mIndicatorGrabbed) {
		mIndicatorGrabbed = false;
		Invalidate();
	}
}

/*------------------------------------------------------------------------------*\
	SetIndicatorPos( pixelPos)
		-	
\*------------------------------------------------------------------------------*/
void BmRulerView::SetIndicatorPos( float pixelPos) {
	pixelPos -= nXOffset;
	int32 newPos = static_cast<int32>( (pixelPos+mSingleCharWidth/2) / mSingleCharWidth);
	if (newPos != mIndicatorPos) {
		mIndicatorPos = newPos;
		Invalidate();
		BView* parent = Parent();
		if (parent && Looper()) {
			BMessage msg( BM_RULERVIEW_NEW_POS);
			msg.AddInt32( MSG_NEW_POS, newPos);
			Looper()->PostMessage( &msg, parent);
		}
	}
}
