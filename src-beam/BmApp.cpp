/*
	BmApp.cpp
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
#include <AppFileInfo.h>
#include <Beep.h>
#include <Deskbar.h>
#include <Roster.h>
#include <Screen.h>

#include <PictureButton.h>

#include "regexx.hh"
using namespace regexx;

#include "BubbleHelper.h"
#include "ImageAboutWindow.h"

#include "BmApp.h"
#include "BmBasics.h"
#include "BmBodyPartView.h"
#include "BmDataModel.h"
#include "BmDeskbarView.h"
#include "BmJobStatusWin.h"
#include "BmLogHandler.h"
#include "BmMailEditWin.h"
#include "BmMailFolderList.h"
#include "BmMailRef.h"
#include "BmMailView.h"
#include "BmMailViewWin.h"
#include "BmMainWindow.h"
#include "BmMsgTypes.h"
#include "BmNetUtil.h"
#include "BmPeople.h"
#include "BmPopAccount.h"
#include "BmPrefs.h"
#include "BmResources.h"
#include "BmSignature.h"
#include "BmSmtpAccount.h"
#include "BmStorageUtil.h"
#include "BmUtil.h"

int BmApplication::InstanceCount = 0;

BmApplication* bmApp = NULL;

static const char* BM_BEEP_EVENT = "New E-mail";

const char* BM_APP_SIG = "application/x-vnd.zooey-beam";
const char* const BM_DeskbarItemName = "Beam_DeskbarItem";

/*------------------------------------------------------------------------------*\
	BmApplication()
		-	constructor
\*------------------------------------------------------------------------------*/
BmApplication::BmApplication( const char* sig)
	:	inherited( sig)
	,	mIsQuitting( false)
	,	mInitCheck( B_NO_INIT)
	,	mMailWin( NULL)
	,	mPrintSetup( NULL)
	,	mPrintJob( "Mail")
{
	if (InstanceCount > 0)
		throw BM_runtime_error("Trying to initialize more than one instance of class Beam");

	bmApp = this;

	try {
		BmAppName = bmApp->Name();
		// set version info:
		app_info appInfo;
		BFile appFile;
		version_info vInfo;
		BAppFileInfo appFileInfo;
		bmApp->GetAppInfo( &appInfo); 
		appFile.SetTo( &appInfo.ref, B_READ_ONLY);
		appFileInfo.SetTo( &appFile);
		if (appFileInfo.GetVersionInfo( &vInfo, B_APP_VERSION_KIND) == B_OK) {
			BmAppVersion = vInfo.short_info;
		}
		BmAppNameWithVersion = BmAppName + " " + BmAppVersion;

		// create the log-handler:
		node_ref nref;
		nref.device = appInfo.ref.device;
		nref.node = appInfo.ref.directory;
		BmLogHandler::CreateInstance( 1, &nref);

		// load/determine all needed resources:
		BmResources::CreateInstance();
		time_t appModTime;
		appFile.GetModificationTime( &appModTime);
		TheResources->CheckMimeTypeFile( sig, appModTime);

		// load the preferences set by user (if any):
		BmPrefs::CreateInstance();
		
		// create the node-monitor looper:
		BmMailMonitor::CreateInstance();

		// create the job status window:
		BmJobStatusWin::CreateInstance();
		TheJobStatusWin->Hide();
		TheJobStatusWin->Show();

		BmSignatureList::CreateInstance();
		TheSignatureList->StartJobInThisThread();

		BmPeopleList::CreateInstance();
		BmMailFolderList::CreateInstance();
		BmSmtpAccountList::CreateInstance( TheJobStatusWin);
		BmPopAccountList::CreateInstance( TheJobStatusWin);

		add_system_beep_event( BM_BEEP_EVENT);

		BmMainWindow::CreateInstance();
		
		BString wspc = ThePrefs->GetString( "Workspace", "Current");
		TheMainWindow->SetWorkspaces( wspc=="Current" 
													? B_CURRENT_WORKSPACE 
													: 1<<(atoi( wspc.String())-1));
		
		TheBubbleHelper.EnableHelp( ThePrefs->GetBool( "ShowTooltips", true));

		mInitCheck = B_OK;
		InstanceCount++;
	} catch (exception& err) {
		ShowAlert( err.what());
		exit( 10);
	}
}

/*------------------------------------------------------------------------------*\
	~BmApplication()
		-	standard destructor
\*------------------------------------------------------------------------------*/
BmApplication::~BmApplication() {
	RemoveDeskbarItem();
	ThePeopleList = NULL;
	TheMailFolderList = NULL;
	ThePopAccountList = NULL;
	TheSmtpAccountList = NULL;
	TheSignatureList = NULL;
#ifdef BM_REF_DEBUGGING
	BmRefObj::PrintRefsLeft();
#endif
//	TheLogHandler->ShutDown();
	delete mPrintSetup;
	delete ThePrefs;
	delete TheResources;
	delete TheLogHandler;
	InstanceCount--;
}

/*------------------------------------------------------------------------------*\
	ReadyToRun()
		-	ensures that main-window is visible
		-	if Beam has been instructed to show a specific mail on startup, the
			mail-window is shown on top of the main-window
\*------------------------------------------------------------------------------*/
void BmApplication::ReadyToRun() {
	if (TheMainWindow->IsMinimized())
		TheMainWindow->Minimize( false);
	if (mMailWin) {
		TheMainWindow->SendBehind( mMailWin);
		mMailWin = NULL;
	}
}

/*------------------------------------------------------------------------------*\
	Run()
		-	starts Beam
\*------------------------------------------------------------------------------*/
thread_id BmApplication::Run() {
	if (InitCheck() != B_OK) {
		exit(10);
	}
	thread_id tid = 0;
	try {
		if (ThePrefs->GetBool( "UseDeskbar"))
			InstallDeskbarItem();
		TheSmtpAccountList->StartJobInThisThread();
		TheMainWindow->Show();
		tid = inherited::Run();
		ThePopAccountList->Store();
		TheSmtpAccountList->Store();
#ifdef BM_DEBUG_MEM
		(new BAlert( "", "End of Beam(1), check mem-usage!!!", "OK"))->Go();
		ThePopAccountList = NULL;
		(new BAlert( "", "End of Beam(2), check mem-usage!!!", "OK"))->Go();
		TheSmtpAccountList = NULL;
		(new BAlert( "", "End of Beam(3), check mem-usage!!!", "OK"))->Go();
		TheMailFolderList = NULL;
		(new BAlert( "", "End of Beam(4), check mem-usage!!!", "OK"))->Go();
#endif
	} catch( exception &e) {
		BM_SHOWERR( e.what());
		exit(10);
	}
	return tid;
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmApplication::InstallDeskbarItem() {
	if (!mDeskbar.HasItem( BM_DeskbarItemName)) {
		status_t res;
		app_info appInfo;
		if ((res = bmApp->GetAppInfo( &appInfo)) == B_OK) {
			int32 id;
			appInfo.ref.set_name( BM_DeskbarItemName);
			if ((res = mDeskbar.AddItem( &appInfo.ref, &id)) != B_OK)
				BM_SHOWERR( BString("Unable to install Beam_DeskbarItem.\nError: \n\t") << strerror( res))
		}
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmApplication::RemoveDeskbarItem() {
	if (mDeskbar.HasItem( BM_DeskbarItemName))
		mDeskbar.RemoveItem( BM_DeskbarItemName);
}

/*------------------------------------------------------------------------------*\
	QuitRequested()
		-	standard BeOS-behaviour, we allow a quit
\*------------------------------------------------------------------------------*/
bool BmApplication::QuitRequested() {
	mIsQuitting = true;
	TheMailMonitor->LockLooper();
	bool shouldQuit = true;
	// ask all windows if they are ready to quit, in which case we actually do quit 
	// (only if *ALL* windows indicate that they are prepared to quit!):
	int32 count = CountWindows();
	for( int32 i=count-1; shouldQuit && i>=0; --i) {
		BWindow* win = bmApp->WindowAt( i);
		if (win && !win->QuitRequested())
			shouldQuit = false;
	}

	if (!shouldQuit) {
		TheMailMonitor->UnlockLooper();
		mIsQuitting = false;
	} else {
		TheMailMonitor->Quit();
		for( int32 i=count-1; i>=0; --i) {
			BWindow* win = bmApp->WindowAt( i);
			if (win) {
				win->LockLooper();
				win->Quit();
			}
		}
	}
	return shouldQuit;
}

/*------------------------------------------------------------------------------*\
	ArgvReceived( argc, argv)
		-	first argument is interpreted to be a destination mail-address, so a 
			new mail is generated for if an argument has been provided
\*------------------------------------------------------------------------------*/
void BmApplication::ArgvReceived( int32 argc, char** argv) {
	if (argc>1) {
		BMessage msg(BMM_NEW_MAIL);
		BString to( argv[1]);
		if (to.ICompare("mailto:",7)==0) {
			to.Remove( 0, 7);
			int32 optPos = to.IFindFirst("?");
			if (optPos != B_ERROR) {
				BString opts;
				to.MoveInto( opts, optPos, to.Length());
				Regexx rx;
				int32 optCount = rx.exec( opts, "[?&]([^&=]+)=([^&]+)", Regexx::global);
				for( int i=0; i<optCount; ++i) {
					BString rawVal;
					DeUrlify( rx.match[i].atom[1], rawVal);
					msg.AddString( MSG_OPT_FIELD, rx.match[i].atom[0]);
					msg.AddString( MSG_OPT_VALUE, rawVal);
				}
			}
		}
		msg.AddString( MSG_WHO_TO, to.String());
		PostMessage( &msg);
	}
}

/*------------------------------------------------------------------------------*\
	RefsReceived( msg)
		-	every given reference is opened, draft and pending messages are shown
			in the mail-edit-window, while other mails are opened view-only.
\*------------------------------------------------------------------------------*/
void BmApplication::RefsReceived( BMessage* msg) {
	if (!msg)
		return;
	entry_ref inref;
	entry_ref eref;
	BEntry entry;
	struct stat st;
	for( int index=0; msg->FindRef( "refs", index, &inref) == B_OK; ++index) {
		if (entry.SetTo( &inref, true) != B_OK
		|| entry.GetRef( &eref) != B_OK
		|| entry.GetStat( &st) != B_OK)
			continue;
		BmRef<BmMailRef> ref = BmMailRef::CreateInstance( NULL, eref, st);
		if (!ref)
			continue;
		if (ref->Status() == BM_MAIL_STATUS_DRAFT
		|| ref->Status() == BM_MAIL_STATUS_PENDING) {
			BmMailEditWin* editWin = BmMailEditWin::CreateInstance( ref.Get());
			if (editWin) {
				editWin->Show();
				if (!mMailWin)
					mMailWin = editWin;
			}
		} else {
			BmMailViewWin* viewWin = BmMailViewWin::CreateInstance( ref.Get());
			if (viewWin) {
				viewWin->Show();
				if (!mMailWin)
					mMailWin = viewWin;
			}
		}
	}
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	handles all actions on mailrefs, like replying, forwarding, and creating
			new mails
\*------------------------------------------------------------------------------*/
void BmApplication::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			case BMM_CHECK_ALL: {
				ThePopAccountList->CheckMail( true);
				break;
			}
			case BMM_CHECK_MAIL: {
				const char* key = NULL;
				msg->FindString( BmPopAccountList::MSG_ITEMKEY, &key);
				if (key) {
					bool isAutoCheck = msg->FindBool( BmPopAccountList::MSG_AUTOCHECK);
					BM_LOG( BM_LogPop, BString("PopAccount ") << key << " checks mail " 
							  << (isAutoCheck ? "(auto)" : "(manual)"));
					if (!isAutoCheck || !ThePrefs->GetBool( "AutoCheckOnlyIfPPPRunning", true) 
					|| IsPPPRunning())
						ThePopAccountList->CheckMailFor( key, isAutoCheck);
				} else
					ThePopAccountList->CheckMail();
				break;
			}
			case BMM_NEW_MAIL: {
				BmRef<BmMail> mail = new BmMail( true);
				const char* to = NULL;
				if ((to = msg->FindString( MSG_WHO_TO)))
					mail->SetFieldVal( BM_FIELD_TO, to);
				const char* optField = NULL;
				int32 i=0;
				for( ; msg->FindString( MSG_OPT_FIELD, i, &optField)==B_OK; ++i) {
					mail->SetFieldVal( optField, msg->FindString( MSG_OPT_VALUE, i));
				}
				BM_LOG( BM_LogMainWindow, BString("Asked to create new mail with ") << i << " options");
				BmMailEditWin* editWin = BmMailEditWin::CreateInstance( mail.Get());
				if (editWin)
					editWin->Show();
				break;
			}
			case BMM_REDIRECT: {
				BmMailRef* mailRef = NULL;
				int index=0;
				while( msg->FindPointer( MSG_MAILREF, index++, (void**)&mailRef) == B_OK) {
					BmRef<BmMail> mail = BmMail::CreateInstance( mailRef);
					if (mail) {
						BM_LOG( BM_LogMainWindow, BString("Asked to redirect mail <") << mailRef->TrackerName() << ">");
						mail->StartJobInThisThread( BmMail::BM_READ_MAIL_JOB);
						if (mail->InitCheck() != B_OK)
							continue;
						BmRef<BmMail> newMail;
						newMail = mail->CreateRedirect();
						if (newMail) {
							BmMailEditWin* editWin = BmMailEditWin::CreateInstance( newMail.Get());
							if (editWin)
								editWin->Show();
						}
					}
					mailRef->RemoveRef();	// msg is no more refering to mailRef
				}
				break;
			}
			case BMM_REPLY:
			case BMM_REPLY_LIST:
			case BMM_REPLY_ORIGINATOR:
			case BMM_REPLY_ALL: {
				int32 buttonPressed=1;
				type_code tc;
				int32 msgCount;
				status_t result = msg->GetInfo( MSG_MAILREF, &tc, &msgCount);
				if (result != B_OK)
					break;
				if (msgCount>1 && msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, ask user about how to forward multiple messages:
					BString s("You have selected more than one message.\n\nShould Beam join the message-bodies into one single mail and reply to that or would you prefer to keep the messages separate?");
					BAlert* alert = new BAlert( "Forwarding Multiple Mails", 
														 s.String(),
													 	 "Cancel", "Keep Separate", "Join", B_WIDTH_AS_USUAL,
													 	 B_OFFSET_SPACING, B_IDEA_ALERT);
					alert->SetShortcut( 0, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(*msg), BMessenger( this)));
				} else {
					if (buttonPressed > 0)
						ReplyToMails( msg, buttonPressed==2);
				}
				break;
			}
			case BMM_FORWARD_ATTACHED:
			case BMM_FORWARD_INLINE:
			case BMM_FORWARD_INLINE_ATTACH: {
				int32 buttonPressed=1;
				type_code tc;
				int32 msgCount;
				status_t result = msg->GetInfo( MSG_MAILREF, &tc, &msgCount);
				if (result != B_OK)
					break;
				if (msgCount>1 && msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, ask user about how to forward multiple messages:
					BString s("You have selected more than one message.\n\nShould Beam join the message-bodies into one single mail and forward that or would you prefer to keep the messages separate?");
					BAlert* alert = new BAlert( "Forwarding Multiple Mails", 
														 s.String(),
													 	 "Cancel", "Keep Separate", "Join", B_WIDTH_AS_USUAL,
													 	 B_OFFSET_SPACING, B_IDEA_ALERT);
					alert->SetShortcut( 0, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(*msg), BMessenger( this)));
				} else {
					if (buttonPressed > 0)
						ForwardMails( msg, buttonPressed==2);
				}
				break;
			}
			case BMM_MARK_AS: {
				int32 buttonPressed=1;
				BmMailRef* mailRef = NULL;
				type_code tc;
				int32 msgCount;
				status_t result = msg->GetInfo( MSG_MAILREF, &tc, &msgCount);
				if (result != B_OK)
					break;
				int index=0;
				BString newStatus = msg->FindString( MSG_STATUS);
				if (msgCount>1 && msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, check if user has asked to mark as read and has selected
					// messages with advanced statii (like replied or forwarded). If so, 
					// we ask the user about how to handle those:
					if (newStatus == BM_MAIL_STATUS_READ) {
						bool hasAdvanced = false;
						while( msg->FindPointer( MSG_MAILREF, index++, (void**)&mailRef) == B_OK) {
							if (mailRef->Status() != BM_MAIL_STATUS_NEW
							&&  mailRef->Status() != BM_MAIL_STATUS_READ) {
								hasAdvanced = true;
								break;
							}
						}
						if (hasAdvanced) {
							BString s("Some of the selected messages are not new.\n\nShould Beam mark only the new messages as being read?");
							BAlert* alert = new BAlert( "Mark Mails As", 
																 s.String(),
															 	 "Cancel", "Mark All Messages", "Just Mark New Messages", B_WIDTH_AS_USUAL,
															 	 B_OFFSET_SPACING, B_IDEA_ALERT);
							alert->SetShortcut( 0, B_ESCAPE);
							alert->Go( new BInvoker( new BMessage(*msg), BMessenger( this)));
							break;
						} else
							buttonPressed = 1;	// mark all messages
					} else
						buttonPressed = 1;	// mark all messages
				}
				if (buttonPressed == 0)
					break;
				index=0;
				while( msg->FindPointer( MSG_MAILREF, index++, (void**)&mailRef) == B_OK) {
					if (buttonPressed==1 || mailRef->Status() == BM_MAIL_STATUS_NEW) {
						BM_LOG( BM_LogMainWindow, BString("marking mail <") << mailRef->TrackerName() << "> as " << newStatus);
						mailRef->MarkAs( newStatus.String());
					}
					mailRef->RemoveRef();	// msg is no more refering to mailRef
				}
				break;
			}
			case c_about_window_url_invoked: {
				const char* url;
				if (msg->FindString( "url", &url) == B_OK) {
					LaunchURL( url);
				}
				break;
			}
			case BMM_PAGE_SETUP: {
				PageSetup();
				break;
			}
			case BMM_PRINT: {
				if (!mPrintSetup)
					PageSetup();
				if (mPrintSetup)
					PrintMails( msg);
				break;
			}
			case BMM_TRASH: {
				BmMailRef* mailRef = NULL;
				int index=0;
				while( msg->FindPointer( MSG_MAILREF, index++, (void**)&mailRef) == B_OK);
				entry_ref* refs = new entry_ref [index];
				index=0;
				for( index=0; msg->FindPointer( MSG_MAILREF, index, (void**)&mailRef) == B_OK; ++index) {
					BM_LOG( BM_LogMainWindow, BString("Asked to trash mail <") << mailRef->TrackerName() << ">");
					refs[index] = mailRef->EntryRef();
					mailRef->MarkAs( BM_MAIL_STATUS_READ);
							// mark mail as read before moving to trash, so that we don't confuse
							// our new-mail counter
					mailRef->RemoveRef();	// msg is no more refering to mailRef
				}
				MoveToTrash( refs, index);
				delete [] refs;
				break;
			}
			case B_SILENT_RELAUNCH: {
				BM_LOG2( BM_LogAll, "App: silently relaunched");
				if (TheMainWindow->IsMinimized())
					TheMainWindow->Minimize( false);
				inherited::MessageReceived( msg);
				break;
			}
			case B_QUIT_REQUESTED: 
				BM_LOG2( BM_LogAll, "App: quit requested");
			default:
				inherited::MessageReceived( msg);
				break;
		}
	}
	catch( exception &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BString("BmApp: ") << err.what());
	}
}

/*------------------------------------------------------------------------------*\
	ForwardMails( msg, join)
		-	forwards all mailref's contained in the given message.
		-	depending on the param join, multiple mails are joined into one single 
			forward or one forward is generated for each mail.
\*------------------------------------------------------------------------------*/
void BmApplication::ForwardMails( BMessage* msg, bool join) {
	if (!msg)
		return;
	// tell sending ref-view that we are doing work:
	BMessenger sendingRefView;
	msg->FindMessenger( MSG_SENDING_REFVIEW, &sendingRefView);
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_SET_BUSY);

	BmMailRef* mailRef = NULL;
	BmRef<BmMail> newMail;
	const char* selectedText = NULL;
	msg->FindString( MSG_SELECTED_TEXT, &selectedText);
	if (join) {
		for(  int index=0; 
				msg->FindPointer( MSG_MAILREF, index, (void**)&mailRef) == B_OK;
				++index) {
			BmRef<BmMail> mail = BmMail::CreateInstance( mailRef);
			if (mail) {
				mail->StartJobInThisThread( BmMail::BM_READ_MAIL_JOB);
				if (mail->InitCheck() != B_OK)
					continue;
				if (msg->what == BMM_FORWARD_ATTACHED) {
					if (index == 0) 
						newMail = mail->CreateAttachedForward();
					else
						newMail->AddAttachmentFromRef( mailRef->EntryRefPtr());
				} else if (msg->what == BMM_FORWARD_INLINE) {
					if (index == 0) 
						newMail = mail->CreateInlineForward( false, selectedText);
					else
						newMail->AddPartsFromMail( mail, false, BM_IS_FORWARD);
				} else if (msg->what == BMM_FORWARD_INLINE_ATTACH) {
					if (index == 0) 
						newMail = mail->CreateInlineForward( true, selectedText);
					else
						newMail->AddPartsFromMail( mail, true, BM_IS_FORWARD);
				}
				if (index == 0) {
					// set subject for multiple forwards:
					BString oldSub = mail->GetFieldVal( BM_FIELD_SUBJECT);
					BString subject = newMail->CreateForwardSubjectFor( oldSub + " (and more...)");
					newMail->SetFieldVal( BM_FIELD_SUBJECT, subject);
				}
			}
			mailRef->RemoveRef();	// msg is no more refering to mailRef
		}
		if (newMail) {
			BmMailEditWin* editWin = BmMailEditWin::CreateInstance( newMail.Get());
			if (editWin)
				editWin->Show();
		}
	} else {
		// create one forward per mail:
		for(  int index=0; 
				msg->FindPointer( MSG_MAILREF, index, (void**)&mailRef) == B_OK;
				++index) {
			BmRef<BmMail> mail = BmMail::CreateInstance( mailRef);
			if (mail) {
				mail->StartJobInThisThread( BmMail::BM_READ_MAIL_JOB);
				if (mail->InitCheck() != B_OK)
					continue;
				if (msg->what == BMM_FORWARD_ATTACHED) {
					newMail = mail->CreateAttachedForward();
				} else if (msg->what == BMM_FORWARD_INLINE) {
					newMail = mail->CreateInlineForward( false, index==0 ? selectedText : NULL);
				} else if (msg->what == BMM_FORWARD_INLINE_ATTACH) {
					newMail = mail->CreateInlineForward( true, index==0 ? selectedText : NULL);
				}
				if (newMail) {
					BmMailEditWin* editWin = BmMailEditWin::CreateInstance( newMail.Get());
					if (editWin)
						editWin->Show();
				}
			}
			mailRef->RemoveRef();		// msg is no more refering to mailRef
		}
	}
	// now tell sending ref-view that we are finished:
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_UNSET_BUSY);
}

/*------------------------------------------------------------------------------*\
	ReplyToMails( msg, join)
		-	replies to all mailref's contained in the given message.
		-	depending on the param join, multiple mails are joined into one single 
			reply (one per originator) or one reply is generated for each mail.
\*------------------------------------------------------------------------------*/
void BmApplication::ReplyToMails( BMessage* msg, bool join) {
	if (!msg)
		return;
	// tell sending ref-view that we are doing work:
	BMessenger sendingRefView;
	msg->FindMessenger( MSG_SENDING_REFVIEW, &sendingRefView);
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_SET_BUSY);

	BmMailRef* mailRef = NULL;
	const char* selectedText = NULL;
	msg->FindString( MSG_SELECTED_TEXT, &selectedText);
	if (join) {
		typedef map< BString, BmRef<BmMail> >BmNewMailMap;
		BmNewMailMap newMailMap;
		// we collect new mails in the map defined above...
		for(  int index=0; 
				msg->FindPointer( MSG_MAILREF, index, (void**)&mailRef) == B_OK;
				++index) {
			BmRef<BmMail> mail = BmMail::CreateInstance( mailRef);
			if (mail) {
				mail->StartJobInThisThread( BmMail::BM_READ_MAIL_JOB);
				if (mail->InitCheck() != B_OK)
					continue;
				BString replyAddr = mail->DetermineReplyAddress( msg->what, true);
				BmRef<BmMail>& newMail = newMailMap[replyAddr];
				if (!newMail) 
					newMail = mail->CreateReply( msg->what, selectedText);
				else
					newMail->AddPartsFromMail( mail, false, BM_IS_REPLY);
				if (index == 0) {
					// set subject for multiple forwards:
					BString oldSub = mail->GetFieldVal( BM_FIELD_SUBJECT);
					BString subject = newMail->CreateReplySubjectFor( oldSub + " (and more...)");
					newMail->SetFieldVal( BM_FIELD_SUBJECT, subject);
				}
			}
			mailRef->RemoveRef();	// msg is no more refering to mailRef
		}
		// ...and now show all the freshly generated mails:
		BmNewMailMap::const_iterator iter;
		for( iter=newMailMap.begin(); iter!=newMailMap.end(); ++iter) {
			BmMailEditWin* editWin = BmMailEditWin::CreateInstance( iter->second.Get());
			if (editWin)
				editWin->Show();
		}
	} else {
		// create one reply per mail:
		BmRef<BmMail> newMail;
		for(  int index=0; 
				msg->FindPointer( MSG_MAILREF, index, (void**)&mailRef) == B_OK;
				++index) {
			BmRef<BmMail> mail = BmMail::CreateInstance( mailRef);
			if (mail) {
				mail->StartJobInThisThread( BmMail::BM_READ_MAIL_JOB);
				if (mail->InitCheck() != B_OK)
					continue;
				newMail = mail->CreateReply( msg->what, index==0 ? selectedText : NULL);
				if (newMail) {
					BmMailEditWin* editWin = BmMailEditWin::CreateInstance( newMail.Get());
					if (editWin)
						editWin->Show();
				}
			}
			mailRef->RemoveRef();		// msg is no more refering to mailRef
		}
	}
	// now tell sending ref-view that we are finished:
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_UNSET_BUSY);
}

/*------------------------------------------------------------------------------*\
	PrintMails( msg)
		-	prints all mailref's contained in the given message.
\*------------------------------------------------------------------------------*/
void BmApplication::PrintMails( BMessage* msg) {
	if (!mPrintSetup)
		PageSetup();
	if (!mPrintSetup)
		return;

	// tell sending ref-view that we are doing work:
	BMessenger sendingRefView;
	msg->FindMessenger( MSG_SENDING_REFVIEW, &sendingRefView);
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_SET_BUSY);

	mPrintJob.SetSettings( new BMessage( *mPrintSetup));
	status_t result = mPrintJob.ConfigJob();
	if (result == B_OK) {
		delete mPrintSetup;
		mPrintSetup = mPrintJob.Settings();
		int32 firstPage = mPrintJob.FirstPage();
		int32 lastPage = mPrintJob.LastPage();
		if (lastPage-firstPage+1 <= 0)
			return;
		BmMailRef* mailRef = NULL;
		// we create a hidden mail-view-window which is being used for printing:
		BmMailViewWin* mailWin = BmMailViewWin::CreateInstance();
		mailWin->Hide();
		mailWin->Show();
		BmMailView* mailView = mailWin->MailView();
		// now get printable rect...
		BRect printableRect = mPrintJob.PrintableRect();
		// ...and adjust mailview accordingly (to use the available space effectively):
		mailView->LockLooper();
		mailWin->ResizeTo( printableRect.Width()+8, 600);
		mailView->UnlockLooper();
		mailView->BodyPartView()->IsUsedForPrinting( true);
		// now we start printing...
		mPrintJob.BeginJob();
		int32 page = 1;
		for(  int mailIdx=0; 
				msg->FindPointer( MSG_MAILREF, mailIdx, (void**)&mailRef) == B_OK
				&& page<=lastPage;
				++mailIdx) {
			mailView->ShowMail( mailRef, false);
			mailView->LockLooper();
			mailView->JobIsDone( true);
			mailView->BodyPartView()->SetViewColor( White);
			mailView->BodyPartView()->JobIsDone( true);
			mailView->UnlockLooper();
			BRect currFrame = printableRect.OffsetToCopy( 0, 0);
			BRect textRect = mailView->TextRect();
			float totalHeight = textRect.top+mailView->TextHeight(0,100000);
			float height = currFrame.Height();
			BPoint topOfLine = mailView->PointAt( mailView->OffsetAt( BPoint( 5, currFrame.bottom)));
			currFrame.bottom = topOfLine.y-1;
			while( page<=lastPage) {
				if (page >= firstPage) {
					mPrintJob.DrawView( mailView, currFrame, BPoint(0,0));
					mPrintJob.SpoolPage();
				}
				currFrame.top = currFrame.bottom+1;
				currFrame.bottom = currFrame.top + height-1;
				mailView->LockLooper();
				topOfLine = mailView->PointAt( mailView->OffsetAt( BPoint( 5, currFrame.bottom)));
				mailView->UnlockLooper();
				currFrame.bottom = topOfLine.y-1;
				page++;
				if (currFrame.top >= totalHeight || currFrame.Height() <= 1)
					// end of current mail reached
					break;
			}
			mailRef->RemoveRef();	// msg is no more refering to mailRef
		}
		mPrintJob.CommitJob();
		mailWin->PostMessage( B_QUIT_REQUESTED);
	}
	// now tell sending ref-view that we are finished:
	if (sendingRefView.IsValid())
		sendingRefView.SendMessage( BMM_UNSET_BUSY);
}

/*------------------------------------------------------------------------------*\
	PageSetup()
		-	sets up the basic printing environment
\*------------------------------------------------------------------------------*/
void BmApplication::PageSetup() {
	if (mPrintSetup)
		mPrintJob.SetSettings( new BMessage( *mPrintSetup));
		
	status_t result = mPrintJob.ConfigPage();
	if (result == B_OK) {
		delete mPrintSetup;
		mPrintSetup = mPrintJob.Settings();
	}
}

/*------------------------------------------------------------------------------*\
	LaunchURL( url)
		-	launches the corresponding program for the given URL (usually Netpositive)
		-	mailto: - URLs are handled internally
\*------------------------------------------------------------------------------*/
void BmApplication::LaunchURL( const BString url) {
	char* urlStr = const_cast<char*>( url.String());
	status_t result;
	if (url.ICompare( "https:", 6) == 0)
		result = be_roster->Launch( "application/x-vnd.Be.URL.https", 1, &urlStr);
	else if (url.ICompare( "http:", 5) == 0)
		result = be_roster->Launch( "application/x-vnd.Be.URL.http", 1, &urlStr);
	else if (url.ICompare( "ftp:", 4) == 0)
		result = be_roster->Launch( "application/x-vnd.Be.URL.ftp", 1, &urlStr);
	else if (url.ICompare( "file:", 5) == 0)
		result = be_roster->Launch( "application/x-vnd.Be.URL.file", 1, &urlStr);
	else if (url.ICompare( "mailto:", 7) == 0) {
		BMessage msg(BMM_NEW_MAIL);
		BString to( urlStr+7);
		int32 subjPos = to.IFindFirst("?subject=");
		if (subjPos != B_ERROR) {
			msg.AddString( BmApplication::MSG_SUBJECT, to.String()+subjPos+9);
			to.Truncate( subjPos);
		}
		msg.AddString( BmApplication::MSG_WHO_TO, to.String());
		bmApp->PostMessage( &msg);
		return;
	}
	else result = B_ERROR;
	if (!(result == B_OK || result == B_ALREADY_RUNNING)) {
		(new BAlert( "", (BString("Could not launch application for url\n\t") 
								<< url << "\n\nError:\n\t" << strerror(result)).String(),
					   "OK"))->Go();
	}
}

/*------------------------------------------------------------------------------*\
	AboutRequested()
		-	standard BeOS-behaviour, we show about-window
\*------------------------------------------------------------------------------*/
void BmApplication::AboutRequested() {
	ImageAboutWindow* aboutWin = new ImageAboutWindow(
		"About Beam",
		"Beam",
		TheResources->IconByName("AboutIcon"),
		15, 
		"BEware, Another Mailer\n(c) Oliver Tappe, Berlin, Germany",
		"mailto:beam@hirschkaefer.de",
		"http://www.hirschkaefer.de/beam",
		"\n\n\n\n\n\n\nThanks to:

Heike Herfart 
	for understanding the geek, 
	the testing sessions 
	and many, many suggestions. 


...and (in alphabetical order):

Adam McNutt
Atillâ Öztürk
Bernd Korz
Cedric Vincent
Charlie Clark
Eberhard Hafermalz
Eugenia Loli-Queru
Helmar Rudolph
Jace Cavacini
Kevin Musick
Lars Müller (of SuSE)
Linus Almstrom
Mathias Reitinger
Max Hartmann
MDR-team (MailDaemonReplacement)
qwilk
Rainer Riedl
Rob Lund
Shard
Stephan Buelling
Stephen Butters
Tyler Dauwalder
Zach 

\n\n\n\n
...and thanks to everyone I forgot, too!

\n\n\n\n\n\n"
	);
	aboutWin->Show();
}

/*------------------------------------------------------------------------------*\
	ScreenFrame()
		-	returns the the current screen's frame
\*------------------------------------------------------------------------------*/
BRect BmApplication::ScreenFrame() {
	BScreen screen;
	if (!screen.IsValid())
		BM_SHOWERR( BString("Could not initialize BScreen object !?!"));
	return screen.Frame();
}

/*------------------------------------------------------------------------------*\
	SetNewWorkspace( newWorkspace)
		-	ensures that main-window and job-status-window are always shown inside
			the same workspace
\*------------------------------------------------------------------------------*/
void BmApplication::SetNewWorkspace( uint32 newWorkspace) {
	if (TheMainWindow->Workspaces() != newWorkspace)
		TheMainWindow->SetWorkspaces( newWorkspace);
	if (TheJobStatusWin->Workspaces() != newWorkspace)
		TheJobStatusWin->SetWorkspaces( newWorkspace);
}

/*------------------------------------------------------------------------------*\
	HandlesMimetype( mimetype)
		-	determines whether or not Beam handles the given mimetype
		-	returns true for text/x-email and message/rfc822, false otherwise
\*------------------------------------------------------------------------------*/
bool BmApplication::HandlesMimetype( const BString mimetype) {
	return mimetype.ICompare( "text/x-email")==0 
			 || mimetype.ICompare( "message/rfc822")==0;
}
