/*
 * Copyright 2002-2006, project beam (http://sourceforge.net/projects/beam).
 * All rights reserved. Distributed under the terms of the GNU GPL v2.
 *
 * Authors:
 *		Oliver Tappe <beam@hirschkaefer.de>
 */

#include <ctype.h>

#include "regexx.hh"
#include "split.hh"
using namespace regexx;

#include "BmBasics.h"
#include "BmEncoding.h"
using namespace BmEncoding;
#include "BmLogHandler.h"
#include "BmPrefs.h"
#include "BmUtil.h"

#undef BM_LOGNAME
#define BM_LOGNAME "MailParser"

const int16 BmEncoding::BM_MAX_HEADER_LINE_LEN = 76;
							// as stated in [K. Johnson, p. 149f]
const int16 BmEncoding::BM_MAX_BODY_LINE_LEN = 998;
							// RFC 2822

BmCharsetMap BmEncoding::TheCharsetMap;

BmString BmEncoding::DefaultCharset = "iso-8859-15";

static iconv_t ICONV_ERR = (iconv_t)0xFFFFFFFF;

// handle different constness in iconv() on haiku and BeOS:
#ifdef __HAIKU__
#define ICONV_IN_BUF(x) const_cast<char**>(x)
#else
#define ICONV_IN_BUF(x) x
#endif

/*------------------------------------------------------------------------------*\
	HandleOneCharset()
		-	this function is called during initialization of libiconv.
		-	for every passed charset (which can have more than one name) Beam 
			checks if it prefers one of the given names over all the other 
			(according to some regular expressions). 
			If so, the name Beam prefers is used, the first name otherwise.
\*------------------------------------------------------------------------------*/
static int HandleOneCharset( unsigned int namescount, 
									  const char * const * names,
                             void* /*data*/) {
	// the preferred names of charsets:
	static const char* rxs[] = {
		"^US-ASCII$",
		"^ISO-",
		"^WINDOWS-",
		"^CP",
		"^KOI8-",
		"^EUC-",
		"^MACROMAN$",
		"^SHIFT-JIS$",
		"^UTF-",
		"^UCS-",
		NULL
	};
	Regexx rx;
	int index=0;
	for( int r=0; rxs[r]!=NULL; ++r) {
		rx.expr( rxs[r]);
		for( uint32 i=0; i<namescount; ++i) {
			rx.str( names[i]);
			if (rx.exec() > 0) {
				index = i;
				goto out;
			}
		}
	}
out:
	for( uint32 i=0; i<namescount; ++i)
		TheCharsetMap[names[i]] = false;
	TheCharsetMap[names[index]] = true;
	return 0;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::InitCharsetMap() {
	iconvlist( &HandleOneCharset, NULL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::GetPreferredCharsets( BmCharsetVect& charsetVect, 
													const BmString& nativeCharset,
													bool outbound)
{
	charsetVect.clear();
	BmString autoCharsets 
		= ThePrefs->GetString( 
				outbound 
					? "AutoCharsetsOutbound"
					: "AutoCharsetsInbound"
		  );
	BmCharsetVect autoCharsetVect;
	split( BmPrefs::nListSeparator, autoCharsets, autoCharsetVect);
	// add given native-charset (as specified in body/header) as first entry:
	charsetVect.push_back(nativeCharset);
	for( uint32 i=0; i<autoCharsetVect.size(); ++i) {
		if (autoCharsetVect[i].ICompare("default") == 0)
			charsetVect.push_back(ThePrefs->GetString("DefaultCharset"));
		else if (autoCharsetVect[i].ICompare(nativeCharset) != 0)
			charsetVect.push_back(autoCharsetVect[i]);
	}
	// add native charset as last, too, as this simplifies showing
	// the appropriate error-msg:
	charsetVect.push_back(nativeCharset);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
const char* BmEncoding::ConvertFromBeosToLibiconv( uint32 encoding) {
	static const char* map[] = {
		"iso-8859-1",
		"iso-8859-2",
		"iso-8859-3",
		"iso-8859-4",
		"iso-8859-5",
		"iso-8859-6",
		"iso-8859-7",
		"iso-8859-8",
		"iso-8859-9",
		"iso-8859-10",
		"macroman",
		"shift-jis",
		"euc-jp",
		"iso-2022-jp-2",
		"windows-1252",
		"ucs-4",
		"koi8-r",
		"windows-1251",
		"cp866",
		"cp850",
		"euc-kr",
		"iso-8859-13",
		"iso-8859-14",
		"iso-8859-15",
		"utf-8"
	};
	if (encoding >= 0 && encoding <= 24)
		return map[encoding];
	else
		return "utf-8";
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::ConvertToUTF8( const BmString& srcCharset, 
										  const BmString& src,
										  BmString& dest) {
	if (srcCharset == "utf-8") {
		// source already is utf-8...
		dest = src;
		return;
	}
	BmStringIBuf srcBuf( src);
	const uint32 blockSize = std::max( (int32)128, src.Length());
	BmStringOBuf destBuf( blockSize);
	BmUtf8Encoder encoder( &srcBuf, srcCharset, blockSize);
	destBuf.Write( &encoder, blockSize);
	dest.Adopt( destBuf.TheString());
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::ConvertFromUTF8( const BmString& destCharset, 
											 const BmString& src, 
											 BmString& dest) {
	if (destCharset == "utf-8") {
		// destination shall be utf-8, too...
		dest = src;
		return;
	}
	BmStringIBuf srcBuf( src);
	const uint32 blockSize = std::max( (int32)128, src.Length());
	BmStringOBuf destBuf( blockSize);
	BmUtf8Decoder decoder( &srcBuf, destCharset, blockSize);
	destBuf.Write( &decoder, blockSize);
	dest.Adopt( destBuf.TheString());
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::Encode( BmString encodingStyle, const BmString& src, 
								 BmString& dest, const BmString& tags) {
	BmStringIBuf srcBuf( src);
	const uint32 blockSize = std::max( (int32)128, src.Length());
	BmStringOBuf destBuf( blockSize);
	BmMemFilterRef encoder 
		= FindEncoderFor( &srcBuf, encodingStyle, blockSize, tags);
	destBuf.Write( encoder.get(), blockSize);
	dest.Adopt( destBuf.TheString());
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmEncoding::Decode( BmString encodingStyle, const BmString& src, 
								 BmString& dest, const BmString& tags) {
	BmStringIBuf srcBuf( src);
	const uint32 blockSize = std::max( (int32)128, src.Length());
	BmStringOBuf destBuf( blockSize);
	BmMemFilterRef decoder 
		= FindDecoderFor( &srcBuf, encodingStyle, blockSize, tags);
	destBuf.Write( decoder.get(), blockSize);
	dest.Adopt( destBuf.TheString());
}

struct BmTextPart {
	BmString charset;
	BmString text;
	BmString encodingStyle;
	BmTextPart()								{ }
	BmTextPart( const BmString& cs, const BmString& t, const BmString& es)
		:	charset( cs)
		,	text( t)
		,	encodingStyle( es) 				{ }
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmString BmEncoding::ConvertHeaderPartToUTF8( const BmString& headerPart, 
															 const BmString& defaultCharset,
															 bool& hadConversionError) {
	int32 nm;
	Regexx rx;
	rx.expr( "=\\?(.+?)\\?(.)\\?(.*?)\\?=");
	rx.str( headerPart);
	const uint32 blockSize = std::max( (int32)128, headerPart.Length());
	vector< BmTextPart> textPartVect;
	BmTextPart currTextPart( defaultCharset, "", "");
	
	hadConversionError = false;

	BmStringOBuf result( blockSize, 2.0);
	if ((nm = rx.exec( Regexx::global))!=0) {
		Regexx rxWhite;
		int32 len=headerPart.Length();
		int32 curr=0;
		vector<RegexxMatch>::const_iterator i;
		for( i = rx.match.begin(); i != rx.match.end(); ++i) {
			if (curr < i->start()) {
				// copy the characters between start/curr match and next match,
				// unless it's all whitespace (rfc2047 requires whitespace between
				// encoded words to be removed):
				BmString chars( headerPart.String()+curr, i->start()-curr);
				if (!rxWhite.exec( chars, "^\\s*$")) {
					if (currTextPart.charset != defaultCharset 
					|| currTextPart.encodingStyle != "") {
						// current text-part doesn't fit this text, we need to create
						// a new text-part:
						if (currTextPart.text.Length())
							// add current text-part to vector before creating new one:
							textPartVect.push_back( currTextPart);
						currTextPart.charset = defaultCharset;
						currTextPart.text = "";
						currTextPart.encodingStyle = "";
					}
					currTextPart.text.Append( chars);
				}
			}
			// convert the match (an encoded word) into UTF8:
			BmString srcCharset( i->atom[0]);
			if (!srcCharset.Length() || !srcCharset.ICompare("unknown-8bit"))
				// avoid empty charsets and the (dummy) charset "unknown-8bit"
				srcCharset = defaultCharset;
			const BmString srcEncodingStyle( i->atom[1]);
			BmString chars( headerPart.String()+i->atom[2].start(), 
								 i->atom[2].Length());
			if (currTextPart.charset != srcCharset 
			|| currTextPart.encodingStyle != srcEncodingStyle) {
				// current text-part doesn't fit this text, we need to create
				// a new text-part:
				if (currTextPart.text.Length())
					// add current text-part to vector before creating new one:
					textPartVect.push_back( currTextPart);
				currTextPart.charset = srcCharset;
				currTextPart.text = "";
				currTextPart.encodingStyle = srcEncodingStyle;
			}
			currTextPart.text.Append( chars);
			curr = i->start()+i->Length();
		}
		if (curr<len) {
			// copy the remaining characters,
			// unless it's all whitespace (rfc2047 requires whitespace between
			// encoded words to be removed):
			BmString chars( headerPart.String()+curr, len-curr);
			if (!rxWhite.exec( chars, "^\\s*$")) {
				if (currTextPart.charset != defaultCharset 
				|| currTextPart.encodingStyle != "") {
					// current text-part doesn't fit this text, we need to create
					// a new text-part:
					if (currTextPart.text.Length())
						// add current text-part to vector before creating new one:
					textPartVect.push_back( currTextPart);
					currTextPart.charset = defaultCharset;
					currTextPart.text = "";
					currTextPart.encodingStyle = "";
				}
				currTextPart.text.Append( chars);
			}
		}
	} else {
		// no encoded words neccessary, we just copy the header-part:
		currTextPart.charset = defaultCharset;
		currTextPart.text = headerPart;
		currTextPart.encodingStyle = "";
	}
	if (currTextPart.text.Length())
		// add current text-part to vector before iterating through vector:
		textPartVect.push_back( currTextPart);
	// now step over all text-parts and decode them, concatenating the resulting
	// strings:
	for( uint32 i=0; i<textPartVect.size(); ++i) {
		BmTextPart& textPart = textPartVect[i];
		BmCharsetVect charsetVect;
		// we try the native charset first and (in case of errors)
		// all preferred charsets:
		if (ThePrefs->GetBool("AutoCharsetDetectionInbound", true))
			GetPreferredCharsets( charsetVect, textPart.charset);
		else
			charsetVect.push_back(textPart.charset);
		BmString charset;
		for( uint32 i=0; i<charsetVect.size(); ++i) {
			charset = charsetVect[i];
			BmStringOBuf utf8( blockSize, 2.0);
			BM_LOG2( BM_LogMailParse, 
						BmString( "ConvertHeaderPartToUTF8(): trying charset ") 
							<< charset);
			if (textPart.encodingStyle == "") {
				// no decoding neccessary, just character-conversion:
				BmStringIBuf text( textPart.text);
				BmUtf8Encoder textConverter( &text, charset);
				utf8.Write( &textConverter);
				if (textConverter.HadError() || textConverter.HadToDiscardChars())
					hadConversionError = true;
				else {
					result.Write(utf8.TheString());
					break;
				}
			} else {
				// encoded-words, need decoding + character-conversion:
				BmStringIBuf text( textPart.text);
				BmString tags = BmQuotedPrintableDecoder::nTagIsEncodedWord;
				BmMemFilterRef decoder 
					= FindDecoderFor(&text, textPart.encodingStyle, blockSize, tags);
				BmUtf8Encoder textConverter( decoder.get(), charset, blockSize);
				utf8.Write( &textConverter, blockSize);
				if (textConverter.HadError() || textConverter.HadToDiscardChars())
					hadConversionError = true;
				else {
					result.Write(utf8.TheString());
					break;
				}
			}
		}
	}
	return result.TheString();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmString BmEncoding::ConvertUTF8ToHeaderPart( const BmString& utf8Text, 
															 const BmString& inCharset,
															 bool useQuotedPrintableIfNeeded,
															 int32 fieldLen) {
	bool needsQuotedPrintable = false;
	BmString transferEncoding = "7bit";
	if (useQuotedPrintableIfNeeded)
		needsQuotedPrintable = NeedsQuotedPrintableEncoding( utf8Text);
	if (ThePrefs->GetBool( "Allow8BitMimeInHeader", false) 
	&& needsQuotedPrintable) {
		// use 8bitmime instead of quoted-printable (problematic in headers!)
		transferEncoding = "8bit";
		needsQuotedPrintable = false;
	}
	const uint32 blockSize = std::max( (int32)128, utf8Text.Length());
	BmString foldedString;
	BmCharsetVect charsetVect;
	if (ThePrefs->GetBool("AutoCharsetDetectionOutbound", true)) {
		// we try the native charset first and (in case of errors)
		// all preferred charsets:
		GetPreferredCharsets(charsetVect, inCharset, true);
	} else
		charsetVect.push_back(inCharset);
	BmString charset;
	for( uint32 i=0; i<charsetVect.size(); ++i) {
		charset = charsetVect[i];
		BM_LOG2( BM_LogMailParse, 
					BmString( "ConvertUTF8ToHeaderPart(): trying charset ") 
						<< charset);
		BmStringIBuf srcBuf( utf8Text);
		BmStringOBuf tempIO( blockSize, 2);
		if (needsQuotedPrintable) {
			// encoded-words (quoted-printable) are neccessary, since 
			// headerfield contains non-ASCII chars:
			BmQpEncodedWordEncoder qpEncoder( &srcBuf, blockSize, fieldLen+2, 	
														 charset);
			tempIO.Write( &qpEncoder);
			if (qpEncoder.HadError() || qpEncoder.HadToDiscardChars()) {
				if (i+1 == charsetVect.size())
					throw BM_text_error( "", "", qpEncoder.FirstDiscardedPos());
			} else {
				foldedString.Adopt( tempIO.TheString());
				break;
			}
		} else {
			BmUtf8Decoder textConverter( &srcBuf, charset);
			BmFoldedLineEncoder foldEncoder( &textConverter, BM_MAX_HEADER_LINE_LEN, 
														blockSize, fieldLen+2);
			tempIO.Write( &foldEncoder);
			if (textConverter.HadError() || textConverter.HadToDiscardChars()) {
				if (i+1 == charsetVect.size())
					throw BM_text_error( "", "", textConverter.FirstDiscardedPos());
			} else {
				foldedString.Adopt( tempIO.TheString());
				break;
			}
		}
	}
	return foldedString;		
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmEncoding::NeedsQuotedPrintableEncoding( const BmString& utf8String, 
															  uint16 maxLineLen) {
	// check if string needs quoted-printable/base64 encoding;
	// encoding is needed: 
	// 	- if the string contains non-ASCII chars
	// 	- if the string contains lines longer than the given maxLineLen
	const char* lastNlPos = utf8String.String()-1;
	for( const char* p = utf8String.String(); *p; ++p) {
		signed char c = *p;
		if (c=='\n') {
			if (maxLineLen && p - lastNlPos > maxLineLen)
				// line is too long and needs to be encoded:
				return true;
			lastNlPos = p;
		} else if (c<32 && c!='\r' && c!='\t')
			// N.B.: This is a signed char, so c<32 means [0-31] and [128-255]
			return true;
	}
	return maxLineLen 
			 && (utf8String.String()+utf8String.Length()-lastNlPos > maxLineLen);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmEncoding::IsCompatibleWithText( const BmString& s) {
	// check if given string contains any characters that suggest the data
	// is in fact binary:
	for( int32 i=0; i<s.Length(); ++i) {
		// currently, we believe that only ascii-0 is not compatible
		// with attachments of type text:
		if (!s[i])
			return false;
	}
	return true;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMemFilterRef BmEncoding::FindDecoderFor( BmMemIBuf* input, 
														 const BmString& encodingStyle, 
														 uint32 blockSize,
														 const BmString& tags) {
	BmMemFilter* filter = NULL;
	if (encodingStyle.ICompare("q")==0 
	|| encodingStyle.ICompare("quoted-printable")==0)
		filter = new BmQuotedPrintableDecoder( input, blockSize, tags);
	else if (encodingStyle.ICompare("b")==0 
	|| encodingStyle.ICompare("base64")==0)
		filter = new BmBase64Decoder( input, blockSize);
	else if (encodingStyle.ICompare("7bit")==0 
	|| encodingStyle.ICompare("8bit")==0)
		filter = new BmLinebreakDecoder( input, blockSize);
	else if (encodingStyle.ICompare("binary")==0)
		filter = new BmBinaryDecoder( input, blockSize);
	else {
		filter = new BmLinebreakDecoder( input, blockSize);
		filter->AddStatusText( 
			BmString("FindDecoderFor(): Unrecognized encoding-style <")
				<< encodingStyle << "> found.\nAssuming 7bit."
		);
	}
	return BmMemFilterRef( filter);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMemFilterRef BmEncoding::FindEncoderFor( BmMemIBuf* input, 
														 const BmString& encodingStyle,
														 uint32 blockSize, 
														 const BmString& tags) {
	BmMemFilter* filter = NULL;
	if (encodingStyle.ICompare("q")==0 
	|| encodingStyle.ICompare("quoted-printable")==0)
		filter = new BmQuotedPrintableEncoder( input, blockSize);
	else if (encodingStyle.ICompare("b")==0 
	|| encodingStyle.ICompare("base64")==0)
		filter = new BmBase64Encoder( input, blockSize, tags);
	else if (encodingStyle.ICompare("7bit")==0 
	|| encodingStyle.ICompare("8bit")==0)
		filter = new BmLinebreakEncoder( input, blockSize);
	else if (encodingStyle.ICompare("binary")==0)
		filter = new BmBinaryEncoder( input, blockSize);
	else {
		filter = new BmLinebreakEncoder( input, blockSize);
		filter->AddStatusText( 
			BmString("FindEncoderFor(): Unrecognized encoding-style <")
				<< encodingStyle << "> found.\nAssuming 7bit."
		);
	}
	return BmMemFilterRef( filter);
}




/********************************************************************************\
	BmUtf8Decoder
\********************************************************************************/

const char* BmUtf8Decoder::nTagTransliterate = "<Translit>";
const char* BmUtf8Decoder::nTagDiscard = "<Discard>";

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmUtf8Decoder::BmUtf8Decoder( BmMemIBuf* input, const BmString& destCharset, 
										uint32 blockSize, const BmString& tags)
	:	inherited( input, blockSize, tags)
	,	mDestCharset( destCharset)
	,	mIconvDescr( ICONV_ERR)
	,	mHadToDiscardChars( false)
	,	mFirstDiscardedPos( -1)
	,	mStoppedOnMultibyte( false)
{
	if (mDestCharset.ICompare("utf8")==0)
		// common mistake: utf8 instead of utf-8:
		mDestCharset = "utf-8";

	InitConverter();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmUtf8Decoder::~BmUtf8Decoder()
{
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
}

/*------------------------------------------------------------------------------*\
	Reset()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::Reset( BmMemIBuf* input) {
	inherited::Reset( input);
	InitConverter();
	mHadToDiscardChars = false;
	mFirstDiscardedPos = -1;
	mStoppedOnMultibyte = false;
}

/*------------------------------------------------------------------------------*\
	InitConverter()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::InitConverter() {
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
	BmString flag;
	if (IsTagSet(nTagTransliterate))
		flag = "//TRANSLIT";
	else if (IsTagSet(nTagDiscard))
		flag = "//IGNORE";
	BmString toSet = mDestCharset	+ flag;
	if (!mDestCharset.Length()
	|| (mIconvDescr = iconv_open( toSet.String(), "utf-8")) == ICONV_ERR) {
		AddStatusText( BmString("libiconv: unable to convert from utf-8 to ") 
								<< toSet);
		mHadError = true;
		return;
	}
}

/*------------------------------------------------------------------------------*\
	SetTransliterate()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::SetTransliterate( bool transliterate) {
	if (mIconvDescr != ICONV_ERR && !SetTag( nTagTransliterate, transliterate))
		return;
	InitConverter();
}

/*------------------------------------------------------------------------------*\
	SetDiscard()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::SetDiscard( bool discard) {
	if (mIconvDescr != ICONV_ERR && !SetTag( nTagDiscard, discard))
		return;
	InitConverter();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::Filter( const char* srcBuf, uint32& srcLen, 
									 char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to decode utf8 of ") << srcLen << " bytes");

	const char* inBuf = srcBuf;
	size_t inBytesLeft = srcLen;
	char* outBuf = destBuf;
	size_t outBytesLeft = destLen;
	size_t irrevCount = iconv( mIconvDescr, ICONV_IN_BUF(&inBuf), 
										&inBytesLeft, &outBuf, &outBytesLeft);
	srcLen -= inBytesLeft;
	destLen -= outBytesLeft;
	if (irrevCount == (size_t)-1) {
		if (errno == E2BIG)
			BM_LOG3( BM_LogMailParse, 
						"Result in utf8-decode: too big, need to continue");
		else if (errno == EINVAL) {
			if (mStoppedOnMultibyte) {
				AddStatusText( "utf8-decode: encountered incomplete multibyte "
									"character, parts of text may be missing");
				mHadError = true;
			} else {
				mStoppedOnMultibyte = true;
				BM_LOG3( BM_LogMailParse, 
							"Result in utf8-decode: stopped on multibyte char, "
							"need to continue");
			}
		} else if (errno == EILSEQ) {
			BM_LOG( BM_LogMailParse, 
					  "Result in utf8-decode: invalid multibyte char found");
			if (!mHadToDiscardChars) {
				mFirstDiscardedPos = mSrcCount+srcLen;
				int on = 1;
				iconvctl( mIconvDescr, ICONV_SET_DISCARD_ILSEQ, &on);
				mHadToDiscardChars = true;
			}
		} else {
			BM_SHOWERR( BmString("Unknown error-result in utf8-decode: ") 
								<< errno);
			mHadError = true;
		}
	} else
		mStoppedOnMultibyte = false;

	BM_LOG3( BM_LogMailParse, "utf8-decode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Decoder::Finalize( char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, "finalizing utf8-decoding");

	const char* inBuf = NULL;
	size_t inBytesLeft = 0;
	char* outBuf = destBuf;
	size_t outBytesLeft = destLen;
	size_t irrevCount = iconv( mIconvDescr, ICONV_IN_BUF(&inBuf), 
										&inBytesLeft, &outBuf, &outBytesLeft);
	destLen -= outBytesLeft;
	if (irrevCount == (size_t)-1) {
		if (errno == E2BIG) {
			BM_LOG3( BM_LogMailParse, 
						"Result in utf8-decode: too big, need to continue");
			return;
		}
	}
	mIsFinalized = true;

	BM_LOG3( BM_LogMailParse, "utf8-decode: done");
}



/********************************************************************************\
	BmUtf8Encoder
\********************************************************************************/

const char* BmUtf8Encoder::nTagTransliterate = "<Translit>";
const char* BmUtf8Encoder::nTagDiscard = "<Discard>";

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmUtf8Encoder::BmUtf8Encoder( BmMemIBuf* input, const BmString& srcCharset, 
										uint32 blockSize, const BmString& tags)
	:	inherited( input, blockSize, tags)
	,	mSrcCharset( srcCharset)
	,	mIconvDescr( ICONV_ERR)
	,	mHadToDiscardChars( false)
	,	mFirstDiscardedPos( -1)
	,	mStoppedOnMultibyte( false)
	,	mHaveResetToInitialState( false)
{
	if (mSrcCharset.ICompare("utf8")==0)
		// common mistake: utf8 instead of utf-8:
		mSrcCharset = "utf-8";

	InitConverter();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmUtf8Encoder::~BmUtf8Encoder()
{
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
}

/*------------------------------------------------------------------------------*\
	Reset()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Encoder::Reset( BmMemIBuf* input) {
	inherited::Reset( input);
	InitConverter();
	mHadToDiscardChars = false;
	mFirstDiscardedPos = -1;
	mStoppedOnMultibyte = false;
	mHaveResetToInitialState = false;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Encoder::InitConverter() { 
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
	BmString flag;
	if (IsTagSet(nTagTransliterate))
		flag = "//TRANSLIT";
	else if (IsTagSet(nTagDiscard))
		flag = "//IGNORE";
	BmString toSet = BmString("utf-8") + flag;
	if (!mSrcCharset.Length()
	|| (mIconvDescr = iconv_open( toSet.String(), 
											mSrcCharset.String())) == ICONV_ERR) {
		BM_LOG( BM_LogMailParse,
				  BmString("libiconv: unable to convert from ") 
							<< mSrcCharset << " to " << toSet);
		mHadError = true;
		return;
	}
}

/*------------------------------------------------------------------------------*\
	SetTransliterate()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Encoder::SetTransliterate( bool transliterate) {
	if (mIconvDescr != ICONV_ERR && !SetTag( nTagTransliterate, transliterate))
		return;
	InitConverter();
}

/*------------------------------------------------------------------------------*\
	SetDiscard()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Encoder::SetDiscard( bool discard) {
	if (mIconvDescr != ICONV_ERR && !SetTag( nTagDiscard, discard))
		return;
	InitConverter();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmUtf8Encoder::Filter( const char* srcBuf, uint32& srcLen, 
									 char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to encode utf8 of ") << srcLen << " bytes");

	const char* inBuf = srcBuf;
	size_t inBytesLeft = srcLen;
	char* outBuf = destBuf;
	size_t outBytesLeft = destLen;
	size_t irrevCount = iconv( mIconvDescr, ICONV_IN_BUF(&inBuf), 
										&inBytesLeft, &outBuf, &outBytesLeft);
	srcLen -= inBytesLeft;
	destLen -= outBytesLeft;
	if (irrevCount == (size_t)-1) {
		if (errno == E2BIG)
			BM_LOG3( BM_LogMailParse, 
						"Result in utf8-encode: too big, need to continue");
		else if (errno == EINVAL) {
			if (mStoppedOnMultibyte) {
				if (!mHaveResetToInitialState) {
					// return to inital state:
					iconv( mIconvDescr, NULL, NULL, NULL, NULL);
					mHaveResetToInitialState = true;
				} else {
					AddStatusText( "utf8-encode: encountered incomplete multibyte "
										"character, parts of text may be missing");
					mHadError = true;
				}
			} else {
				mStoppedOnMultibyte = true;
				mHaveResetToInitialState = false;
				BM_LOG3( BM_LogMailParse, 
							"Result in utf8-encode: stopped on multibyte char, "
							"need to continue");
			}
		} else if (errno == EILSEQ) {
			BM_LOG2( BM_LogMailParse, 
						"Result in utf8-encode: invalid multibyte char found");
			if (!mHadToDiscardChars) {
				mFirstDiscardedPos = mSrcCount + srcLen;
				int on = 1;
				iconvctl( mIconvDescr, ICONV_SET_DISCARD_ILSEQ, &on);
				mHadToDiscardChars = true;
			}
		} else {
			BM_LOG( BM_LogMailParse,
					  BmString("Unknown error-result in utf8-encode: ") 
								<< errno);
			mHadError = true;
		}
	} else
		mStoppedOnMultibyte = false;

	BM_LOG3( BM_LogMailParse, "utf8-encode: done");
}



/********************************************************************************\
	BmQuotedPrintableDecoder
\********************************************************************************/

inline unsigned char HEXDIGIT2CHAR( unsigned char d) {
	return (unsigned char)((d>='0' && d<='9') 
					? d-'0' 
					: (d>='A' && d<='F') 
							? d-'A'+10
							: (d>='a' && d<='f') 
									? d-'a'+10
									: '\0');
}

const char* BmQuotedPrintableDecoder::nTagIsEncodedWord = "<EncWord>";

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmQuotedPrintableDecoder::BmQuotedPrintableDecoder( BmMemIBuf* input, 
																	 uint32 blockSize,
																	 const BmString& tags)
	:	inherited( input, blockSize, tags)
	,	mSpacesThatMayNeedRemoval( 0)
	,	mSoftbreakPending( false)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQuotedPrintableDecoder::Filter( const char* srcBuf, uint32& srcLen, 
													char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to decode quoted-printable of ") 
						<< srcLen << " bytes");
	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	char c,c1,c2;
	const BmString qpChars("abcdef0123456789ABCDEF");
	for( ; src<srcEnd && dest<destEnd; ++src) {
		c = *src;
		if (c == '\r') {
			// skip over carriage-returns:
			continue;
		}
		if (mSoftbreakPending) {
			if (c == '\n') {
				// softbreak, skip over "\n" in order to join the line:
				mSoftbreakPending = false;
				mSpacesThatMayNeedRemoval = 0;
				continue;
			} else if (c != ' ') {
				// some broken encoding, we insert the equal-sign...
				*dest++ = '=';
				// ...unset softbreak-mode...
				mSoftbreakPending = false;
				// ...and re-process the current char:
				--src;
				continue;
			}
		}
		if (mSpacesThatMayNeedRemoval && c != ' ') {
			if (c=='\n') {
				// this group of spaces lives immediately before a newline,
				// which means that it needs to be removed:
				mSpacesThatMayNeedRemoval = 0;
			} else {
				// this group of spaces does not require removal, so it will
				// be added as all other chars:
				while( dest<destEnd && mSpacesThatMayNeedRemoval--)
					*dest++ = ' ';
				if (mSpacesThatMayNeedRemoval < 0)
					mSpacesThatMayNeedRemoval = 0;
			}
			if (dest>=destEnd)
				break;
		}
		if (c == ' ')
			mSpacesThatMayNeedRemoval++;
		else {
			mSoftbreakPending = false;
			mSpacesThatMayNeedRemoval = 0;
			if (c == '=') {
				if (src>srcEnd-3 && !mInput->IsAtEnd())
					break;						// want two more characters in buffer
				if (src<=srcEnd-3 && (c1=*(src+1))!=0 && (c2=*(src+2))!=0) {
					if (qpChars.FindFirst(c1)!=B_ERROR 
					&& qpChars.FindFirst(c2)!=B_ERROR) {
						// decode a single character:
						*dest++ = char(HEXDIGIT2CHAR(c1)*16 + HEXDIGIT2CHAR(c2));
						src += 2;
					} else {
						// it's either a softbreak or broken encoding, we'll see:
						mSoftbreakPending = true;
					}
				} else {
					// characters missing at end (broken encoding), we just copy:
					*dest++ = c;
				}
			} else if (IsTagSet(nTagIsEncodedWord) && c == '_') {
				// in encoded-words, underlines are really spaces 
				// (a real underline is encoded):
				*dest++ = ' ';
			} else {
				// anything else is just copied:
				*dest++ = c;
			}
		}
	}
	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "qp-decode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQuotedPrintableDecoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	if (mSoftbreakPending && dest < destEnd) {
		// broken encoding, we need to insert the equal-sign...
		*dest++ = '=';
		mSoftbreakPending = false;
	}
	destLen = dest-destBuf;
	mIsFinalized = !mSoftbreakPending;
}



/********************************************************************************\
	BmQuotedPrintableEncoder
\********************************************************************************/

inline unsigned int CHAR2HIGHNIBBLE( unsigned char c) {
	return (((c) > 0x9F ? 'A'-10 : '0')+((c)>>4));
}
inline unsigned int CHAR2LOWNIBBLE( unsigned char c) {
	return ((((c)&0x0F) > 9 ? 'A'-10 : '0')+((c)&0x0F));
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmQuotedPrintableEncoder::BmQuotedPrintableEncoder( BmMemIBuf* input, 
																	 uint32 blockSize)
	:	inherited( input, blockSize)
	,	mQueuedChars( 80)
	,	mSpacesThatMayNeedEncoding( 0)
	,	mLastAddedLen( 0)
	,	mCurrAddedLen( 0)
	,	mNeedFlush( false)
	,	mKeepLen( 0)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQuotedPrintableEncoder::Queue( const char* chars, uint32 len) {
	mQueuedChars.Put( chars, len);
	mLastAddedLen = mCurrAddedLen;
	mCurrAddedLen = len;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmQuotedPrintableEncoder::OutputLineIfNeeded( char* &dest, 
																	const char* destEnd) {
	if (!mNeedFlush && mQueuedChars.Length() > BM_MAX_HEADER_LINE_LEN) {
		// line is too long and needs soft break:
		mKeepLen 
			= (mQueuedChars.Length() - mCurrAddedLen < BM_MAX_HEADER_LINE_LEN)
				? mCurrAddedLen
						// last char-group fits on line, so we include it.
				: mLastAddedLen + mCurrAddedLen;
						// last char-group won't fit, so we need to fold
						// before it.
		mNeedFlush = true;
	}
	if (mNeedFlush) {
		// line is too long or needs hard break, we output all chars up to
		// the group added last:
		if (mQueuedChars.PeekTail() != '\n') {
			while (mQueuedChars.Length() > mKeepLen) {
				if (dest>=destEnd)
					return false;
				*dest++ = mQueuedChars.Get();
			}
			// insert soft linebreak:
			if (dest+3>=destEnd)
				return false;
			*dest++ = '=';
			*dest++ = '\r';
			*dest++ = '\n';
		} else {
			while (mQueuedChars.Length() > 0) {
				if (dest>=destEnd)
					return false;
				*dest++ = mQueuedChars.Get();
			}
		}
		mNeedFlush = false;
	}
	return true;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQuotedPrintableEncoder::Filter( const char* srcBuf, uint32& srcLen, 
													char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to encode quoted-printable of ") 
						<< srcLen << " bytes");
	const char* safeChars = 
				(ThePrefs->GetBool( "MakeQPSafeForEBCDIC", false)
					? "%&/()?+*,.;:<>-_"
					: "%&/()?+*,.;:<>-_!\"#$@[]\\^'{|}~");
							// in bodies, the underscore is safe, i.e. it need
							// not be encoded.
	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	char c;
	for( ; src<srcEnd && dest<destEnd; ++src) {
		if (!OutputLineIfNeeded( dest, destEnd))
			break;
		c = *src;
		if (c=='\r')
			continue;							// ignore '\r'
		if (mSpacesThatMayNeedEncoding && c!=' ') {
			// now we can decide if this group of spaces need to be encoded
			// or not:
			if (c=='\n') {
				// this group of spaces lives immediately before a newline,
				// which means that it requires encoding:
				Queue( "=20", 3);
				mSpacesThatMayNeedEncoding--;
			} else {
				// no encoding neccessary for this group of spaces:
				Queue( " ", 1);
				mSpacesThatMayNeedEncoding--;
			}
			--src;								// reprocess current character
			continue;
		} else {
			// normal processing, convert chars as needed and queue them:
			if (isalnum(c) || strchr( safeChars, c)) {
				Queue( &c, 1);
			} else if (c == ' ') {
				mSpacesThatMayNeedEncoding++;
			} else if (c=='\n') {
				Queue( "\r\n", 2);
				mNeedFlush = true;
			} else {
				char enc[3] = {
					'=',
					(char)CHAR2HIGHNIBBLE(c),
				 	(char)CHAR2LOWNIBBLE(c)
				};
				Queue( enc, 3);
			}
		}
	}
	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "qp-encode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQuotedPrintableEncoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	if (OutputLineIfNeeded( dest, destEnd)) {
		// spaces at end of string are considered to live immediately before 
		// a newline, which means that they require encoding:
		while( mSpacesThatMayNeedEncoding) {
			Queue( "=20", 3);
			mSpacesThatMayNeedEncoding--;
		}
		// output remaining chars (will fit on one line):
		while (mQueuedChars.Length()) {
			if (dest>=destEnd)
				break;
			*dest++ = mQueuedChars.Get();
		}
	}
	destLen = dest-destBuf;
	mIsFinalized = (mQueuedChars.Length()==0 && mSpacesThatMayNeedEncoding==0);
}



/********************************************************************************\
	BmQpEncodedWordEncoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmQpEncodedWordEncoder::BmQpEncodedWordEncoder( BmMemIBuf* input, 
																uint32 blockSize,
																int startAtOffset,
																const BmString& destCharset)
	:	inherited( input, blockSize)
	,	mQueuedChars( 80)
	,	mStartAtOffset( startAtOffset)
	,	mLastAddedLen( 0)
	,	mCurrAddedLen( 0)
	,	mNeedFlush( false)
	,	mKeepLen( 0)
	,	mCharacterLen( 0)
	,	mDestCharset( destCharset)
	,	mIconvDescr( ICONV_ERR)
	,	mHadToDiscardChars( false)
	,	mFirstDiscardedPos( -1)
	,	mStoppedOnMultibyte( false)
{
	if (mDestCharset.ICompare("utf8")==0)
		// common mistake: utf8 instead of utf-8:
		mDestCharset = "utf-8";

	InitConverter();
	//
	mEncodedWord = BmString("=?")<<destCharset<<"?q?";
	mQueuedChars << mEncodedWord;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmQpEncodedWordEncoder::~BmQpEncodedWordEncoder()
{
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
}

/*------------------------------------------------------------------------------*\
	Reset()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::Reset( BmMemIBuf* input) {
	inherited::Reset( input);
	InitConverter();
	mHadToDiscardChars = false;
	mFirstDiscardedPos = -1;
}

/*------------------------------------------------------------------------------*\
	InitConverter()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::InitConverter() {
	if (mIconvDescr != ICONV_ERR) {
		iconv_close( mIconvDescr);
		mIconvDescr = ICONV_ERR;
	}
	BmString toSet = mDestCharset;
	if (!mDestCharset.Length()
	|| (mIconvDescr = iconv_open( toSet.String(), "utf-8")) == ICONV_ERR) {
		AddStatusText( BmString("libiconv: unable to convert from utf-8 to ") 
								<< toSet);
		mHadError = true;
		return;
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::Queue( const char* chars, uint32 len) {
	mQueuedChars.Put( chars, len);
	mLastAddedLen = mCurrAddedLen;
	mCurrAddedLen = len;
	mCharacterLen += len;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmQpEncodedWordEncoder::OutputLineIfNeeded( char* &dest, 
																 const char* destEnd) {
	if (!mNeedFlush 
	&& mStartAtOffset+mQueuedChars.Length()+2 > BM_MAX_HEADER_LINE_LEN) {
		// line is too long and needs soft break:
		mKeepLen 
			= (mStartAtOffset + mQueuedChars.Length() 
				- mCurrAddedLen < BM_MAX_HEADER_LINE_LEN)
				? mCurrAddedLen
						// last char-group fits on line, so we include it.
				: mLastAddedLen + mCurrAddedLen;
						// last char-group won't fit, so we need to fold
						// before it.
		if (mCharacterLen > mKeepLen)
			mKeepLen = mCharacterLen;
							// keep at least the last character (may be multiple bytes)
		mNeedFlush = true;
	}
	if (mNeedFlush) {
		// line is too long or needs hard break, we output all chars up to
		// the group added last:
		while (mQueuedChars.Length() 
		&& mQueuedChars.Length() > mKeepLen) {
			if (dest>=destEnd)
				return false;
			*dest++ = mQueuedChars.Get();
		}
		// insert soft linebreak:
		if (dest+5+mEncodedWord.Length()>=destEnd)
			return false;
		*dest++ = '?';
		*dest++ = '=';
		*dest++ = '\r';
		*dest++ = '\n';
		*dest++ = ' ';
		for( int i=0; i<mEncodedWord.Length(); ++i)
			*dest++ = mEncodedWord[i];
		mNeedFlush = false;
		// note number of chars already added to line:
		mStartAtOffset = mEncodedWord.Length()+1;
	}
	return true;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::EncodeConversionBuf() { 
	const char* safeChars = 
			 (ThePrefs->GetBool( "MakeQPSafeForEBCDIC", false)
					? "%&/+*.-"
					: "%&/+*.-!#$@^{|}");
							// in encoded words, underscore has to be encoded, since
							// it is used for spaces!
							// the question-mark has to be encoded, too, since it
							// is part of the encoded-word markers!
							// the characters ()"'<>;:[] are considered dangerous in 
							// encoded words, so these are encoded, too.
	char c;
	int32 len = mConversionBuf.Length();
	if (len>0) {
		mCharacterLen = 0;
		for( int32 pos=0; pos<len; ++pos) {
			c = mConversionBuf[pos];
			if (isalnum(c) || strchr( safeChars, c)) {
				Queue( &c, 1);
			} else if (c == ' ') {
				// in encoded-words, we always replace SPACE by underline:
				Queue( "_", 1);
			} else if (c == '\r') {
				continue;							// dump '\r'
			} else {
				// qp-encode a single character:
				char enc[3] = {
					'=',
					(char)CHAR2HIGHNIBBLE(c),
				 	(char)CHAR2LOWNIBBLE(c)
				};
				Queue( enc, 3);
			}
		}
		mConversionBuf.Truncate(0);
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::Filter( const char* srcBuf, uint32& srcLen, 
												 char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to qp-word-encode ") << srcLen << " bytes");

	const char* src = srcBuf;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	EncodeConversionBuf();

	if (OutputLineIfNeeded( dest, destEnd)) {
		const char* srcEnd = srcBuf+srcLen;
		size_t srcBytesLeft = 1;
		if (IS_UTF8_STARTCHAR(*srcBuf)) {
			const char* p = srcBuf+1;
			while( p<srcEnd && IS_WITHIN_UTF8_MULTICHAR( *p++))
				srcBytesLeft++;
		}
		const int32 conversionBufLen = 256;
								// a value larger than any imaginable byte-length
								// of a single character (no matter what encoding)!
		char* outBuf = mConversionBuf.LockBuffer( conversionBufLen);
		size_t outBytesLeft = conversionBufLen;
		size_t irrevCount = iconv( mIconvDescr, ICONV_IN_BUF(&src), 
											&srcBytesLeft, &outBuf, &outBytesLeft);
		mConversionBuf.UnlockBuffer( conversionBufLen - outBytesLeft);
		if (irrevCount == (size_t)-1) {
			if (errno == E2BIG)
				BM_LOG3( BM_LogMailParse, 
							"Result in utf8-decode: too big, need to continue");
			else if (errno == EINVAL) {
				if (mStoppedOnMultibyte) {
					AddStatusText( "utf8-decode: encountered incomplete multibyte "
										"character, parts of text may be missing");
					mHadError = true;
				} else {
					mStoppedOnMultibyte = true;
					BM_LOG3( BM_LogMailParse, 
								"Result in utf8-decode: stopped on multibyte char, "
								"need to continue");
				}
			} else if (errno == EILSEQ) {
				BM_LOG( BM_LogMailParse, 
						  "Result in utf8-decode: invalid multibyte char found");
				if (!mHadToDiscardChars) {
					mFirstDiscardedPos = mSrcCount;
					int on = 1;
					iconvctl( mIconvDescr, ICONV_SET_DISCARD_ILSEQ, &on);
					mHadToDiscardChars = true;
				}
			} else {
				BM_SHOWERR( BmString("Unknown error-result in utf8-decode: ") 
									<< errno);
				mHadError = true;
			}
		} else
			mStoppedOnMultibyte = false;
	}
	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "qp-word-encode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmQpEncodedWordEncoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	EncodeConversionBuf();

	if (OutputLineIfNeeded( dest, destEnd)) {
		// output remaining chars (followed by "?="):
		if (mQueuedChars.PeekTail() != '=')
			Queue( "?=", 2);
		while( dest<destEnd && mQueuedChars.Length()) {
			*dest++ = mQueuedChars.Get();
		}
	}
	destLen = dest-destBuf;
	mIsFinalized = (mQueuedChars.Length()==0);
}



/********************************************************************************\
	BmFoldedLineEncoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmFoldedLineEncoder::BmFoldedLineEncoder( BmMemIBuf* input, int lineLen,
														uint32 blockSize, int startAtOffset)
	:	inherited( input, blockSize)
	,	mStartAtOffset( startAtOffset)
	,	mMaxLineLen( lineLen)
	,	mLastSpacePos( -1)
	,	mQueuedChars( 80)
	,	mNeedFlush( false)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmFoldedLineEncoder::OutputLineIfNeeded( char* &dest, 
															 const char* destEnd) {
	if (!mNeedFlush 
	&& mStartAtOffset+mQueuedChars.Length() > BM_MAX_HEADER_LINE_LEN) {
		// line is too long and needs soft break:
		mNeedFlush = true;
	}
	if (mNeedFlush) {
		// line is too long we output chars and insert a soft-break:
		if (mLastSpacePos == -1) {
			// no space in this line, so we output all chars except 
			// the last one (so that line is full):
			while (mQueuedChars.Length() > 1) {
				if (dest>=destEnd)
					return false;
				*dest++ = mQueuedChars.Get();
			}
		} else {
			// output all chars up to the last space:
			while (mLastSpacePos>=0) {
				if (dest>=destEnd)
					return false;
				if (mLastSpacePos>0)
					// output character
					*dest++ = mQueuedChars.Get();
				else
					// suppress space as last character of a line:
					mQueuedChars.Get();
				mLastSpacePos--;
			}
		}
		// insert soft linebreak:
		if (dest+3>=destEnd)
			return false;
		*dest++ = '\r';
		*dest++ = '\n';
		*dest++ = ' ';
		mNeedFlush = false;
		mStartAtOffset = 1;
	}
	return true;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmFoldedLineEncoder::Filter( const char* srcBuf, uint32& srcLen, 
											 char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to line-fold ") << srcLen << " bytes");
	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	unsigned char c;
	for( ; src<srcEnd && dest<destEnd; ++src) {
		if (!OutputLineIfNeeded( dest, destEnd))
			break;
		c = *src;
		if (c == ' ') {
			mLastSpacePos = mQueuedChars.Length();
			mQueuedChars << ' ';
		} else if (c == '\r') {
			continue;							// dump '\r'
		} else {
			mQueuedChars << c;
		}
	}
	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "line-fold: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmFoldedLineEncoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	if (OutputLineIfNeeded( dest, destEnd)) {
		// output remaining chars:
		while( dest<destEnd && mQueuedChars.Length()) {
			*dest++ = mQueuedChars.Get();
		}
	}
	destLen = dest-destBuf;
	mIsFinalized = (mQueuedChars.Length()==0);
}



/********************************************************************************\
	BmBase64Decoder
\********************************************************************************/

const int32 BmBase64Decoder::nBase64Alphabet[256] = {
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
	 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -2, -1, -1,
	 -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
	 -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmBase64Decoder::BmBase64Decoder( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
	,	mConcat( 0)
	,	mIndex( 0)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmBase64Decoder::Filter( const char* srcBuf, uint32& srcLen, 
										char* destBuf, uint32& destLen) {

	BM_LOG3( BM_LogMailParse, 
				BmString("starting to decode base64 of ") << srcLen << " bytes");

	int32 value;
	const unsigned char* src = (const unsigned char*)srcBuf;
	const unsigned char* srcEnd = (const unsigned char*)srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
		
	while( src<srcEnd && dest<=destEnd-3) {
		if ((value = nBase64Alphabet[*src++])<0) {
			if (value == -2) {
				// padding-char ('=') encountered, we flush converted chars...
				if (mIndex) {
					*dest++ = char((mConcat & 0x00ff0000) >> 16);
					if (mIndex > 2)
						*dest++ = char((mConcat & 0x0000ff00) >> 8);
				}
				// ... and reset state:
				mConcat = mIndex = 0;
			}
			continue;
		}
			
		mConcat |= (value << ((3-mIndex)*6));
		
		if (++mIndex == 4) {
			*dest++ = char((mConcat & 0x00ff0000) >> 16);
			*dest++ = char((mConcat & 0x0000ff00) >> 8);
			*dest++ = char((mConcat & 0x000000ff));
			mConcat = mIndex = 0;
		}
	}
	srcLen = src-(unsigned char*)srcBuf;
	destLen = dest-destBuf;

	BM_LOG3( BM_LogMailParse, "base64-decode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmBase64Decoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	if (mIndex > 1) {
		// output remaining characters:
		BM_ASSERT( dest<=destEnd-3);
							// must be the case for mIndex!=0
		*dest++ = char((mConcat & 0x00ff0000) >> 16);
		if (mIndex > 2)
			*dest++ = char((mConcat & 0x0000ff00) >> 8);
		mConcat = mIndex = 0;
	}
	destLen = dest-destBuf;
	mIsFinalized = true;
}



/********************************************************************************\
	BmBase64Encoder
\********************************************************************************/

const char* BmBase64Encoder::nTagOnSingleLine = "<OnSingleLine>";

const char BmBase64Encoder::nBase64Alphabet[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'+',
	'/'
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmBase64Encoder::BmBase64Encoder( BmMemIBuf* input, uint32 blockSize, 
											 const BmString& tags)
	:	inherited( input, blockSize, tags)
	,	mConcat( 0)
	,	mIndex( 0)
	,	mCurrLineLen( 0)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	this code is based on the one used by MDR (MailDaemonReplacement)
\*------------------------------------------------------------------------------*/
void BmBase64Encoder::Filter( const char* srcBuf, uint32& srcLen, 
										char* destBuf, uint32& destLen) {

	BM_LOG3( BM_LogMailParse, 
				BmString("starting to encode base64 of ") << srcLen << " bytes");

	const unsigned char* src = (unsigned char*)srcBuf;
	const unsigned char* srcEnd = (unsigned char*)srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
		
	while( src<srcEnd && dest<=destEnd-6) {
		mConcat |= (*src++ << ((2-mIndex)*8));
		if (++mIndex == 3) {
			*dest++ = nBase64Alphabet[(mConcat >> 18) & 63];
			*dest++ = nBase64Alphabet[(mConcat >> 12) & 63];
			*dest++ = nBase64Alphabet[(mConcat >> 6) & 63];
			*dest++ = nBase64Alphabet[mConcat & 63];
			mConcat = mIndex = 0;
			mCurrLineLen += 4;
			if (!IsTagSet(nTagOnSingleLine) 
			&& mCurrLineLen >= BM_MAX_HEADER_LINE_LEN) {
				*dest++ = '\r';
				*dest++ = '\n';
				mCurrLineLen = 0;
			}
		}
	}
	srcLen = src-(unsigned char*)srcBuf;
	destLen = dest-destBuf;

	BM_LOG3( BM_LogMailParse, "base64-encode: done");
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmBase64Encoder::Finalize( char* destBuf, uint32& destLen) {
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;
	if (mIndex) {
		if (dest<=destEnd-4) {
			// output remaining bytes and pad if neccessary:
			*dest++ = nBase64Alphabet[(mConcat >> 18) & 63];
			*dest++ = nBase64Alphabet[(mConcat >> 12) & 63];
			if (mIndex==2)
				*dest++ = nBase64Alphabet[(mConcat >> 6) & 63];
			else
				*dest++ = '=';
			*dest++ = '=';
			mIsFinalized = true;
		}
	} else
		mIsFinalized = true;
	destLen = dest-destBuf;
}



/********************************************************************************\
	BmLinebreakDecoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmLinebreakDecoder::BmLinebreakDecoder( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmLinebreakDecoder::Filter( const char* srcBuf, uint32& srcLen, 
											char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to decode linebreaks of ") 
					<< srcLen << " bytes");
	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	char c;
	for( ; src<srcEnd && dest<destEnd; ++src) {
		if ((c = *src) != '\r')
			*dest++ = c;
	}

	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "linebreak-decode: done");
}



/********************************************************************************\
	BmLinebreakEncoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmLinebreakEncoder::BmLinebreakEncoder( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmLinebreakEncoder::Filter( const char* srcBuf, uint32& srcLen, 
											char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to encode linebreaks of ") 
					<< srcLen << " bytes");

	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	char c;
	for( ; src<srcEnd && dest<destEnd; ++src) {
		if ((c = *src) == '\n') {
			if (dest>destEnd-2)
				break;
			*dest++ = '\r';
		} 
		if (c != '\r')
			// copy character unless it's '\r' (we dump that):
			*dest++ = c;
	}

	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "linebreak-encode: done");
}



/********************************************************************************\
	BmMailtextCleaner
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmMailtextCleaner::BmMailtextCleaner( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
	,	mLastWasStartOfShiftSpace(false)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmMailtextCleaner::Filter( const char* srcBuf, uint32& srcLen, 
										  char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to clean mailtext of ") 
					<< srcLen << " bytes");
	const char* src = srcBuf;
	const char* srcEnd = srcBuf+srcLen;
	char* dest = destBuf;
	char* destEnd = destBuf+destLen;

	// filter out shift-space (C2A0 in UTF-8) as it confuses BTextView, as well
	// as our own wrapping code. We replace it by a normal space:
	char c;
	for( ; src<srcEnd && dest<destEnd; ++src) {
		switch((c = *src)) {
			case '\xC2':
				mLastWasStartOfShiftSpace = true;
				*dest++ = c;
				break;
			case '\xA0':
				if (mLastWasStartOfShiftSpace) {
					*(dest-1) = '\x20';
					mLastWasStartOfShiftSpace = false;
					break;
				}
				else
					// fall through
			default:
				*dest++ = c;
				mLastWasStartOfShiftSpace = false;
		}
	}

	srcLen = src-srcBuf;
	destLen = dest-destBuf;
	BM_LOG3( BM_LogMailParse, "mailtext-cleaner: done");
}



/********************************************************************************\
	BmBinaryDecoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmBinaryDecoder::BmBinaryDecoder( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmBinaryDecoder::Filter( const char* srcBuf, uint32& srcLen, 
										char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to decode binary of ") << srcLen << " bytes");

	uint32 size = std::min( destLen, srcLen);
	memcpy( destBuf, srcBuf, size);

	srcLen = destLen = size;
	BM_LOG3( BM_LogMailParse, "binary-decode: done");
}



/********************************************************************************\
	BmBinaryEncoder
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmBinaryEncoder::BmBinaryEncoder( BmMemIBuf* input, uint32 blockSize)
	:	inherited( input, blockSize)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmBinaryEncoder::Filter( const char* srcBuf, uint32& srcLen, 
										char* destBuf, uint32& destLen) {
	BM_LOG3( BM_LogMailParse, 
				BmString("starting to encode binary of ") << srcLen << " bytes");

	uint32 size = std::min( destLen, srcLen);
	memcpy( destBuf, srcBuf, size);

	srcLen = destLen = size;
	BM_LOG3( BM_LogMailParse, "binary-encode: done");
}
