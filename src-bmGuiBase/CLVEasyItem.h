//		$Id$
//CLVEasyItem header file

//*** LICENSE ***
//ColumnListView, its associated classes and source code, and the other components of Santa's Gift Bag are
//being made publicly available and free to use in freeware and shareware products with a price under $25
//(I believe that shareware should be cheap). For overpriced shareware (hehehe) or commercial products,
//please contact me to negotiate a fee for use. After all, I did work hard on this class and invested a lot
//of time into it. That being said, DON'T WORRY I don't want much. It totally depends on the sort of project
//you're working on and how much you expect to make off it. If someone makes money off my work, I'd like to
//get at least a little something.  If any of the components of Santa's Gift Bag are is used in a shareware
//or commercial product, I get a free copy.  The source is made available so that you can improve and extend
//it as you need. In general it is best to customize your ColumnListView through inheritance, so that you
//can take advantage of enhancements and bug fixes as they become available. Feel free to distribute the 
//ColumnListView source, including modified versions, but keep this documentation and license with it.


#ifndef _CLV_EASY_ITEM_H_
#define _CLV_EASY_ITEM_H_


//******************************************************************************************************
//**** SYSTEM HEADER FILES
//******************************************************************************************************
#include <List.h>


//******************************************************************************************************
//**** PROJECT HEADER FILES AND CLASS NAME DECLARATIONS
//******************************************************************************************************
#include "SantaPartsForBeam.h"
#include "CLVListItem.h"

class BBitmap;

//******************************************************************************************************
//**** CLVEasyItem CLASS DECLARATION
//******************************************************************************************************
class IMPEXPSANTAPARTSFORBEAM CLVEasyItem : public CLVListItem
{
		static const int8 CLV_STYLE_HIGHLIGHT = 1<<0;
		static const int8 CLV_STYLE_HIGHLIGHT_TOP = 1<<1;
		static const int8 CLV_STYLE_HIGHLIGHT_BOTTOM = 1<<2;
		static const int8 CLV_STYLE_BOLD = 1<<3;
		
	public:
		//Constructor and destructor
		CLVEasyItem(uint32 level, bool superitem, bool expanded, ColumnListView* lv);
		virtual ~CLVEasyItem();

		virtual void SetColumnContent(int column_index, const BBitmap *bitmap, int8 horizontal_offset = 2,
			bool right_justify = false);
		virtual void SetColumnContent(int column_index, const char *text, bool right_justify = false);
		virtual void SetColumnUserTextContent(int column_index, bool right_justify = false);
		const char* GetColumnContentText(int column_index);
		const BBitmap* GetColumnContentBitmap(int column_index);
		virtual void DrawItemColumn(BRect item_column_rect, int32 column_index, bool complete);
		virtual void Update(BView *owner, const BFont *font);
		static int CompareItems(const CLVListItem* a_Item1, const CLVListItem* a_Item2, int32 KeyColumn, int32 col_flags);
		inline float GetTextOffset() {return text_offset;}
		virtual const char* GetUserText(int32 column_index, float column_width) const;

		virtual const int32 GetNumValueForColumn( int32) const 		{ return 0; }
		virtual const time_t GetDateValueForColumn( int32) const 	{ return 0; }
		virtual const bigtime_t GetBigtimeValueForColumn( int32) const 	{ return 0; }

		void Highlight( bool b);
		bool Highlight( );
		void HighlightTop( bool b);
		bool HighlightTop( );
		void HighlightBottom( bool b);
		bool HighlightBottom( );
		void Bold( bool b);
		bool Bold( );

	protected:
		void PrepListsForSet(int column_index);
		void SetStyleFlag( uint8 style, bool on);
		
		BList m_column_content;	//List of char* (full content) or BBitmap*

	protected:
		float text_offset;
		uint8 style_flags;
};


#endif
