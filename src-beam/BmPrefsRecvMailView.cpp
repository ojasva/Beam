/*
 * Copyright 2002-2006, project beam (http://sourceforge.net/projects/beam).
 * All rights reserved. Distributed under the terms of the GNU GPL v2.
 *
 * Authors:
 *		Oliver Tappe <beam@hirschkaefer.de>
 */
#include <Alert.h>
#include <FilePanel.h>
#include <MenuItem.h>
#include <PopUpMenu.h>

#include <HGroup.h>
#include <LayeredGroup.h>
#include <MPopup.h>
#include <MStringView.h>
#include <Space.h>
#include <VGroup.h>

#include "BetterButton.h"
#include "BubbleHelper.h"
#include "Colors.h"
#include "ColumnListView.h"
#include "CLVEasyItem.h"

#include "BmCheckControl.h"
#include "BmFilterChain.h"
#include "BmGuiUtil.h"
#include "BmIdentity.h"
#include "BmImap.h"
#include "BmImapAccount.h"
#include "BmLogHandler.h"
#include "BmMail.h"
#include "BmMailFolderList.h"
#include "BmMenuControl.h"
#include "BmMenuController.h"
#include "BmNetEndpointRoster.h"
#include "BmPopAccount.h"
#include "BmPopper.h"
#include "BmPrefs.h"
#include "BmPrefsRecvMailView.h"
#include "BmPrefsWin.h"
#include "BmRosterBase.h"
#include "BmSignature.h"
#include "BmSmtpAccount.h"
#include "BmTextControl.h"
#include "BmUtil.h"

/********************************************************************************\
	BmRecvAccItem
\********************************************************************************/

enum Columns {
	COL_KEY = 0,
	COL_TYPE,
	COL_CHECK,
	COL_DELETE,
	COL_FILTER_CHAIN,
	COL_SERVER,
	COL_PORT,
	COL_CHECK_INTERVAL,
	COL_ENCRYPTION_TYPE,
	COL_AUTH_METHOD,
	COL_USER,
	COL_PWD
};

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRecvAccItem::BmRecvAccItem( ColumnListView* lv, 
										BmListModelItem* _item)
	:	inherited( lv, _item, false)
{
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRecvAccItem::~BmRecvAccItem() { 
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmRecvAccItem::UpdateView( BmUpdFlags flags, bool redraw, 
										  uint32 updColBitmap) {
	BmRecvAccount* acc = ModelItem();
	if (flags & UPD_ALL) {
		const char* cols[] = {
			acc->Key().String(),
			acc->Type(),
			acc->CheckMail() ? "*" : "",
			acc->DeleteMailFromServer() ? "*" : "",
			acc->FilterChain().String(),
			acc->Server().String(),
			acc->PortNrString().String(),
			acc->CheckIntervalString().String(),
			acc->EncryptionType().String(),
			acc->AuthMethod().String(),
			acc->Username().String(),
			acc->PwdStoredOnDisk() ? "*****":"",
			NULL
		};
		SetTextCols( 0, cols);
		updColBitmap = 0xFFFFFFFF;
	}
	inherited::UpdateView( flags, redraw, updColBitmap);
}



/********************************************************************************\
	BmRecvAccView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRecvAccView::BmRecvAccView( int32 width, int32 height)
	:	inherited( BRect(0,0,float(width-1),float(height-1)), "Beam_RecvAccView", 
					  B_SINGLE_SELECTION_LIST, 
					  false, true)
{
	int32 flags = CLV_SORT_KEYABLE;
	SetViewColor( B_TRANSPARENT_COLOR);
	if (ThePrefs->GetBool("StripedListView"))
		SetStripedBackground( true);
	else 
		flags |= CLV_TELL_ITEMS_WIDTH;

	AddColumn( new CLVColumn( "Account", 80.0, flags, 50.0));
	AddColumn( new CLVColumn( "Type", 50.0, flags, 40.0));
	AddColumn( new CLVColumn( "C", 20.0, flags, 20.0, "(C)heck?"));
	AddColumn( new CLVColumn( "R", 20.0, flags, 20.0, 
									  "(R)emove Mails from Server?"));
	AddColumn( new CLVColumn( "FilterChain", 80.0, flags, 40.0));
	AddColumn( new CLVColumn( "Server", 80.0, flags, 40.0));
	AddColumn( new CLVColumn( "Port", 40.0, 0, 40.0));
	AddColumn( new CLVColumn( "Interval", 40.0, 0, 40.0));
	AddColumn( new CLVColumn( "Encryption", 80.0, flags, 40.0));
	AddColumn( new CLVColumn( "Auth-Method", 80.0, flags, 40.0));
	AddColumn( new CLVColumn( "User", 80.0, flags, 40.0));
	AddColumn( new CLVColumn( "Pwd", 50.0, flags, 40.0));

	SetSortFunction( CLVEasyItem::CompareItems);
	SetSortKey( COL_KEY);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmRecvAccView::~BmRecvAccView() {
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmRecvAccView::CreateListViewItem( BmListModelItem* item,
																	BMessage*) {
	return new BmRecvAccItem( this, item);
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
BmListViewItem* BmRecvAccView::AddModelItem( BmListModelItem* item) {
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
void BmRecvAccView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			default:
				inherited::MessageReceived( msg);
		}
	} catch( BM_error &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("RecvAccView:\n\t") << err.what());
	}
}



/********************************************************************************\
	BmPrefsRecvMailView
\********************************************************************************/

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsRecvMailView::BmPrefsRecvMailView() 
	:	inherited( "Receiving Mail-Accounts (POP3 / IMAP)")
	,	mClientCertPanel( NULL)
{
	MView* view = 
		new VGroup(
			new BetterScrollView( 
				minimax(-1,100,1E5,1E5), 
				mAccListView = new BmRecvAccView( 400, 100),
				BM_SV_H_SCROLLBAR | BM_SV_V_SCROLLBAR | BM_SV_CORNER
				| BM_SV_CAPTION,
				"99 accounts"
			),
			new HGroup(
				mAddPopButton = new BetterButton( "Add POP3-Account", 
													  new BMessage(BM_ADD_POP_ACCOUNT), 
													  this),
				mAddImapButton = new BetterButton( "Add IMAP-Account", 
													   new BMessage(BM_ADD_IMAP_ACCOUNT), 
													   this),
				mRemoveButton = new BetterButton( "Remove Account", 
													  new BMessage( BM_REMOVE_ACCOUNT), 
													  this),
				0
			),
			new Space( minimax(0,10,0,10)),
			new HGroup(
				new VGroup(
					new MBorder( M_LABELED_BORDER, 10, (char*)"Account Info",
						new VGroup(
							mAccountControl = new BmTextControl( "Account name:", 
																			 false, 0, 25),
							new Space( minimax(0,5,0,5)),
							new HGroup( 
								mServerControl = new BmTextControl( "Server / Port:"),
								mPortControl = new BmTextControl( "", false, 0, 8),
								0
							),
							new HGroup( 
								mAuthControl = new BmMenuControl( "Auth-method:", 
																			 new BPopUpMenu("")),
								mCheckAndSuggestButton = new BetterButton(
									"Check and Suggest",
									new BMessage(BM_CHECK_AND_SUGGEST), 
									this, minimax(-1,-1,-1,-1)
								),
								0
							),
							new HGroup( 
								mLoginControl = new BmTextControl( "User / Pwd:"),
								mPwdControl = new BmTextControl( "", false, 0, 8),
								0
							),
							new Space( minimax(0,5,0,5)),
							mHomeFolderControl = new BmMenuControl( 
								"Home-Folder:", 
								new BmMenuController( 
									"", this, 
									new BMessage( BM_HOME_FOLDER_SELECTED), 
									&BmGuiRosterBase::RebuildFolderMenu, 
									BM_MC_SKIP_FIRST_LEVEL
								)
							),
							mFilterChainControl = new BmMenuControl( 
								"Filter-Chain:", 
								new BmMenuController( 
									"", this, 
									new BMessage( BM_FILTER_CHAIN_SELECTED), 
									&BmGuiRosterBase::RebuildFilterChainMenu, 
									BM_MC_ADD_NONE_ITEM | BM_MC_LABEL_FROM_MARKED
								)
							),
							0
						)
					),
					new MBorder( M_LABELED_BORDER, 10, (char*)"Encryption & Security",
						new VGroup(
							mEncryptionControl 
								= new BmMenuControl( "Encryption:", 
															new BPopUpMenu("")),
							new HGroup( 
								mAcceptedCertControl 
									= new BmTextControl("Server Certificate:"),
								mClearAcceptedCertButton 
									= new BetterButton( 
										"Clear", 
										new BMessage(BM_CLEAR_ACCEPTED_CERT), 
										this
									),
								0
							),
							new HGroup( 
								mClientCertControl 
									= new BmTextControl("Client Certificate:"),
								mSelectClientCertButton 
									= new BetterButton( 
										"Select"B_UTF8_ELLIPSIS, 
										new BMessage(BM_CLIENT_CERT_SELECTED), 
										this
									),
								0
							),
							0
						)
					),
					0
				),
				new VGroup(
					new MBorder( M_LABELED_BORDER, 10, (char*)"Options",
						new VGroup(
							mAutoCheckIfPppUpControl = new BmCheckControl( 
								"Automatically check for mails only if PPP is up", 
								new BMessage(BM_CHECK_IF_PPP_UP_CHANGED), 
								this, ThePrefs->GetBool("AutoCheckOnlyIfPPPRunning")
							),
							new Space( minimax(0,10,0,10)),
							mCheckAccountControl = new BmCheckControl( 
								"Include in Manual Check", 
								new BMessage(BM_CHECK_MAIL_CHANGED), 
								this
							),
							new HGroup(
								mCheckEveryControl = new BmCheckControl( 
									"Check automatically every", 
									new BMessage(BM_CHECK_EVERY_CHANGED), 
									this
								),
								mCheckIntervalControl = new BmTextControl( 
									"", false, 4
								),
								mMinutesLabel = new MStringView( "minutes"),
								new Space(),
								0
							),
							new HGroup(
								mRemoveMailControl = new BmCheckControl( 
									"Remove mails from server after", 
									new BMessage(BM_REMOVE_MAIL_CHANGED), 
									this
								),
								mDeleteMailDelayControl = new BmTextControl( 
									"", false, 4
								),
								mDaysLabel = new MStringView( "days"),
								new Space(),
								0
							),
							new Space( minimax(0,10,0,10)),
							mStorePwdControl = new BmCheckControl( 
								"Store password on disk (UNSAFE!)", 
								new BMessage(BM_PWD_STORED_CHANGED), 
								this
							),
							0
						)
					),
					new Space(),
					0
				),
				0
			),
			0
		);
	mGroupView->AddChild( dynamic_cast<BView*>(view));
	
	mPwdControl->TextView()->HideTyping( true);
	
	BmDividable::DivideSame(
		mAccountControl,
		mLoginControl,
		mClientCertControl,
		mServerControl,
		mEncryptionControl,
		mAuthControl,
		mHomeFolderControl,
		mFilterChainControl,
		mAcceptedCertControl,
		NULL
	);
	
	mPortControl->SetDivider( 15);
	mPortControl->ct_mpm.weight = 0.4f;
	mPwdControl->SetDivider( 15);
	mPwdControl->ct_mpm.weight = 0.4f;
	mSelectClientCertButton->ct_mpm = minimax(-1,-1,-1,-1,0);
	mClearAcceptedCertButton->ct_mpm = minimax(-1,-1,-1,-1,0);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
BmPrefsRecvMailView::~BmPrefsRecvMailView() {
	delete mClientCertPanel;
	TheBubbleHelper->SetHelp( mAccListView, NULL);
	TheBubbleHelper->SetHelp( mAccountControl, NULL);
	TheBubbleHelper->SetHelp( mLoginControl, NULL);
	TheBubbleHelper->SetHelp( mPwdControl, NULL);
	TheBubbleHelper->SetHelp( mServerControl, NULL);
	TheBubbleHelper->SetHelp( mPortControl, NULL);
	TheBubbleHelper->SetHelp( mCheckAccountControl, NULL);
	TheBubbleHelper->SetHelp( mCheckEveryControl, NULL);
	TheBubbleHelper->SetHelp( mCheckIntervalControl, NULL);
	TheBubbleHelper->SetHelp( mRemoveMailControl, NULL);
	TheBubbleHelper->SetHelp( mDeleteMailDelayControl, NULL);
	TheBubbleHelper->SetHelp( mStorePwdControl, NULL);
	TheBubbleHelper->SetHelp( mEncryptionControl, NULL);
	TheBubbleHelper->SetHelp( mAuthControl, NULL);
	TheBubbleHelper->SetHelp( mHomeFolderControl, NULL);
	TheBubbleHelper->SetHelp( mFilterChainControl, NULL);
	TheBubbleHelper->SetHelp( mCheckAndSuggestButton, NULL);
	TheBubbleHelper->SetHelp( mAutoCheckIfPppUpControl, NULL);
	TheBubbleHelper->SetHelp( mClientCertControl, NULL);
	TheBubbleHelper->SetHelp( mSelectClientCertButton, NULL);
	TheBubbleHelper->SetHelp( mAcceptedCertControl, NULL);
	TheBubbleHelper->SetHelp( mClearAcceptedCertButton, NULL);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::Initialize() {
	inherited::Initialize();

	TheBubbleHelper->SetHelp( 
		mAccListView, 
		"This listview shows every receiving account you have defined."
	);
	TheBubbleHelper->SetHelp( 
		mAccountControl, 
		"Here you can enter a name for this account.\n"
		"This name is used to identify this account in Beam."
	);
	TheBubbleHelper->SetHelp( 
		mLoginControl, 
		"Here you can enter the username which \n"
		"will be used during authentication."
	);
	TheBubbleHelper->SetHelp( 
		mPwdControl, 
		"Here you can enter the password which \n"
		"will be used during authentication.\n"
		"(You can only edit this field if you\n"
		"checked 'Store Password on Disk')."
	);
	TheBubbleHelper->SetHelp( 
		mClientCertControl, 
		TheNetEndpointRoster->SupportsEncryption() 
		?
		"Some servers require the user to authenticate\n"
		"with a PKCS#12-client-certificate (as part of the SSL/TLS-\n"
		"handshake). Here you can specify the file that contains\n"
		"the client certificate."
		:
		"Certificate support is not available,\n"
		"no addon could be loaded"
	);
	TheBubbleHelper->SetHelp( 
		mSelectClientCertButton, 
		TheNetEndpointRoster->SupportsEncryption() 
		?
		"Pressing this button allows you to select the\n"
		"PKCS#12-file that is going to be used as the client\n"
		"certificate in order to authenticate to the server."
		:
		"Certificate support is not available,\n"
		"no addon could be loaded"
	);
	TheBubbleHelper->SetHelp( 
		mServerControl, 
		"Please enter the full name of the server \n"
		"into this field (e.g. 'pop.xxx.org')."
	);
	TheBubbleHelper->SetHelp( 
		mPortControl, 
		"Please enter the port of the server \n"
		"into this field (usually 110 for POP3 and 143 for IMAP)."
	);
	TheBubbleHelper->SetHelp( 
		mCheckAccountControl, 
		"Check this if you want to check this account \n"
		"when pressing the 'Check'-button."
	);
	TheBubbleHelper->SetHelp( 
		mCheckEveryControl, 
		"Check this if you want to check this account \n"
		"in regular intervals."
	);
	TheBubbleHelper->SetHelp( 
		mCheckIntervalControl, 
		"Here you can enter the interval (in minutes)\n"
		"between automatic checks."
	);
	TheBubbleHelper->SetHelp( 
		mRemoveMailControl, 
		"Checking this makes Beam remove each mail \n"
		"from the server after retrieving them."
	);
	TheBubbleHelper->SetHelp( 
		mDeleteMailDelayControl, 
		"Here you can enter the delay (in days)\n"
		"for downloaded mails to be removed from\n"
		"the server.\n"
		"Enter 0 to have the mails removed immediately."
	);
	TheBubbleHelper->SetHelp( 
		mStorePwdControl, 
		"Checking this allows Beam to store the given \n"
		"password unsafely on disk.\n"
		"If you uncheck this, Beam will ask you for the password\n"
		"everytime you use this account."
	);
	static BmString encrHelp;
	if (TheNetEndpointRoster->SupportsEncryption()) {
		encrHelp 
			= 		"Here you can select the type of encryption to use:\n"
					"<none>	- no encryption.";
		if (TheNetEndpointRoster->SupportsEncryptionType(BmRecvAccount::ENCR_TLS))
			encrHelp 
				<< "\n"
				<<	"<auto>		- means the STARTTLS method will be used if available.\n"
					"STARTTLS	- TLS (Transport Layer Security) encryption on\n"
					"		  the standard port (usually 110 for POP and 143 for IMAP).\n"
					"TLS		- TLS (Transport Layer Security) encryption on\n"
					"		  a special port (usually 995 for POP and 993 for IMAP).";
		if (TheNetEndpointRoster->SupportsEncryptionType(BmRecvAccount::ENCR_SSL))
			encrHelp 
				<< "\n"
				<< "SSL		- SSL (Secure Socket Layer) encryption on\n"
					"		  a special port (usually 995 for POP and 993 for IMAP).";
	} else {
		encrHelp = "Encryption is not available,\n"
					  "no addon could be loaded";
	}
	TheBubbleHelper->SetHelp( 
		mEncryptionControl, encrHelp.String()
	);
	TheBubbleHelper->SetHelp( 
		mAuthControl, 
		"Here you can select the authentication type to use:\n"
		"<auto>		- means the best (safest) available mode is used automatically.\n"
		"DIGEST-MD5	- is safe, neither password nor user are sent in cleartext.\n"
		"CRAM-MD5		- is safe, neither password nor user are sent in cleartext.\n"
		"APOP			- is somewhat safe, password is encrypted (but user isn't).\n"
		"LOGIN		- is unsafe (unless encrypted), password is sent in cleartext.\n"
		"POP3			- is unsafe (unless encrypted), password is sent in cleartext"
	);
	TheBubbleHelper->SetHelp( 
		mHomeFolderControl, 
		"Here you can select the mail-folder where all mails \n"
		"received through this account shall be filed into by default.\n"
		"N.B.: Mail-filters will overrule this setting."
	);
	TheBubbleHelper->SetHelp( 
		mFilterChainControl, 
		"Here you can select the filter-chain to be used \n"
		"for every mail received through this account."
	);
	TheBubbleHelper->SetHelp( 
		mCheckAndSuggestButton, 
		"When you click here, Beam will connect to the server,\n"
		"check which authentication types it supports and select\n"
		"the most secure."
	);
	TheBubbleHelper->SetHelp( 
		mAutoCheckIfPppUpControl, 
		"If you check this, automatic checks take place only if you\n"
		"have a running dialup-connection.\n"
		"If you have a permanent connection to the internet, you MUST\n"
		"uncheck this, otherwise no automatic checks will happen!\n"
		"Please note that this setting is valid for all accounts, not\n"
		"just for this one!"
	);
	TheBubbleHelper->SetHelp( 
		mAcceptedCertControl, 
		"This shows the MD5-fingerprint of the server certificate\n"
		"which has been permanently accepted in one of the preceeding\n"
		"sessions."
	);
	TheBubbleHelper->SetHelp( 
		mClearAcceptedCertButton, 
		"Pressing this button resets the certificate information for\n"
		"this account, such that Beam no longer accepts the server's\n"
		"certificate automatically."
	);

	mAccountControl->SetTarget( this);
	mLoginControl->SetTarget( this);
	mPortControl->SetTarget( this);
	mPwdControl->SetTarget( this);
	mServerControl->SetTarget( this);
	mCheckAccountControl->SetTarget( this);
	mCheckEveryControl->SetTarget( this);
	mCheckIntervalControl->SetTarget( this);
	mRemoveMailControl->SetTarget( this);
	mDeleteMailDelayControl->SetTarget( this);
	mStorePwdControl->SetTarget( this);

	AddItemToMenu( mEncryptionControl->Menu(), 
						new BMenuItem( BM_NoItemLabel.String(), 
											new BMessage(BM_ENCRYPTION_SELECTED)), 
						this);
	if (TheNetEndpointRoster->SupportsEncryptionType(BmRecvAccount::ENCR_TLS)) {
		AddItemToMenu( mEncryptionControl->Menu(), 
							new BMenuItem( BmRecvAccount::ENCR_AUTO, 
												new BMessage(BM_ENCRYPTION_SELECTED)), 
							this);
		AddItemToMenu( mEncryptionControl->Menu(), 
							new BMenuItem( BmRecvAccount::ENCR_STARTTLS, 
												new BMessage(BM_ENCRYPTION_SELECTED)), 
							this);
		AddItemToMenu( mEncryptionControl->Menu(), 
							new BMenuItem( BmRecvAccount::ENCR_TLS, 
												new BMessage(BM_ENCRYPTION_SELECTED)), 
							this);
	}
	if (TheNetEndpointRoster->SupportsEncryptionType(BmRecvAccount::ENCR_SSL)) {
		AddItemToMenu( mEncryptionControl->Menu(), 
							new BMenuItem( BmRecvAccount::ENCR_SSL, 
												new BMessage(BM_ENCRYPTION_SELECTED)), 
							this);
	}

	mAccListView->SetSelectionMessage( new BMessage( BM_SELECTION_CHANGED));
	mAccListView->SetTarget( this);
	mAccListView->StartJob( TheRecvAccountList.Get(), false);
	ShowAccount( -1);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::Activated() {
	inherited::Activated();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::WriteStateInfo() {
	mAccListView->WriteStateInfo();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
bool BmPrefsRecvMailView::SanityCheck() {
	return DoSanityCheck( TheRecvAccountList.Get(), Name());
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::SaveData() {
	TheRecvAccountList->Store();
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::UndoChanges() {
	TheRecvAccountList->ResetToSaved();
	ShowAccount( -1);
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::EncryptionSelected() {
	if (!mCurrAcc)
		return;
	BMenuItem* item = mEncryptionControl->Menu()->FindMarked();
	BmString encryptionType;
	if (item && BM_NoItemLabel != item->Label()) {
		encryptionType = item->Label();
		mCurrAcc->EncryptionType( encryptionType);
	} else {
		mCurrAcc->EncryptionType( "");
	}
	BmString currPort = mPortControl->Text();
	if (encryptionType == BmRecvAccount::ENCR_TLS 
	|| encryptionType == BmRecvAccount::ENCR_SSL) {
		if (currPort == mCurrAcc->DefaultPort(false))
			// auto-switch to encrypted default-port:
			mPortControl->SetText(mCurrAcc->DefaultPort(true));
	} else {
		if (currPort == mCurrAcc->DefaultPort(true))
			// auto-switch to unencrypted default-port:
			mPortControl->SetText(mCurrAcc->DefaultPort(false));
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::AuthTypeSelected() {
	if (!mCurrAcc)
		return;
	BMenuItem* item = mAuthControl->Menu()->FindMarked();
	if (item)
		mCurrAcc->AuthMethod( item->Label());
	else
		mCurrAcc->AuthMethod( "");
}

/*------------------------------------------------------------------------------*\
	MessageReceived( msg)
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::MessageReceived( BMessage* msg) {
	try {
		switch( msg->what) {
			case BM_SELECTION_CHANGED: {
				int32 index = mAccListView->CurrentSelection( 0);
				ShowAccount( index);
				break;
			}
			case BM_TEXTFIELD_MODIFIED: {
				if (mCurrAcc) {
					BView* srcView = NULL;
					msg->FindPointer( "source", (void**)&srcView);
					BmTextControl* source = dynamic_cast<BmTextControl*>( srcView);
					if ( source == mAccountControl) {
						BmString oldName = mCurrAcc->Name();
						BmString newName = mAccountControl->Text();
						TheRecvAccountList->RenameItem( oldName, newName);
						BmRef<BmIdentity> correspIdent;
						BmModelItemMap::const_iterator iter;
						// update any links to this account:
						BAutolock lock( TheIdentityList->ModelLocker());
						for(  iter = TheIdentityList->begin(); 
								iter != TheIdentityList->end(); ++iter) {
							BmIdentity* ident 
								= dynamic_cast<BmIdentity*>( iter->second.Get());
							if (ident) {
								// rename link to this account
								if (ident->RecvAccount()==oldName)
									ident->RecvAccount( newName);
								// take note that we have an identity that shares the
								// name with this account:
								if (ident->Key()==oldName)
									correspIdent = ident;
							}
						}
						// rename identity that shares the name with this account:
						if (correspIdent)
							TheIdentityList->RenameItem( oldName, newName);
						BAutolock lock2( TheSmtpAccountList->ModelLocker());
						for(  iter = TheSmtpAccountList->begin(); 
								iter != TheSmtpAccountList->end(); ++iter) {
							BmSmtpAccount* acc 
								= dynamic_cast<BmSmtpAccount*>( iter->second.Get());
							if (acc && acc->AccForSmtpAfterPop()==oldName)
								acc->AccForSmtpAfterPop( newName);
						}
					} else if ( source == mLoginControl)
						mCurrAcc->Username( mLoginControl->Text());
					else if ( source == mPortControl)
						mCurrAcc->PortNr( uint16(atoi(mPortControl->Text())));
					else if ( source == mPwdControl)
						mCurrAcc->Password( mPwdControl->Text());
					else if ( source == mServerControl)
						mCurrAcc->Server( mServerControl->Text());
					else if ( source == mCheckIntervalControl)
						mCurrAcc->CheckInterval( 
							int16(MAX( 0,atoi(mCheckIntervalControl->Text()))));
					else if ( source == mDeleteMailDelayControl)
						mCurrAcc->DeleteMailDelay( 
							uint16(MAX( 0,atoi(mDeleteMailDelayControl->Text()))));
					else if ( source == mClientCertControl)
						mCurrAcc->ClientCertificate( mClientCertControl->Text());
					NoticeChange();
				}
				break;
			}
			case BM_CHECK_MAIL_CHANGED: {
				if (mCurrAcc) {
					mCurrAcc->CheckMail( mCheckAccountControl->Value());
					NoticeChange();
				}
				break;
			}
			case BM_CHECK_EVERY_CHANGED: {
				bool val = mCheckEveryControl->Value();
				mCheckIntervalControl->SetEnabled( val);
				if (!val) {
					mCheckIntervalControl->SetTextSilently( "");
					if (mCurrAcc)
						mCurrAcc->CheckInterval( 0);
				}
				mMinutesLabel->SetHighColor( 
					tint_color( 
						ui_color( B_PANEL_TEXT_COLOR), 
						val ? B_NO_TINT: B_DISABLED_MARK_TINT
					)
				);
				mMinutesLabel->Invalidate();
				NoticeChange();
				break;
			}
			case BM_REMOVE_MAIL_CHANGED: {
				if (mCurrAcc) {
					bool val = mRemoveMailControl->Value();
					mCurrAcc->DeleteMailFromServer( val);
					mDeleteMailDelayControl->SetEnabled( val);
					if (!val) {
						mDeleteMailDelayControl->SetTextSilently( "");
						if (mCurrAcc)
							mCurrAcc->DeleteMailDelay( 0);
					}
					mDaysLabel->SetHighColor(
						tint_color( 
							ui_color( B_PANEL_TEXT_COLOR), 
							val ? B_NO_TINT: B_DISABLED_MARK_TINT
						)
					);
					mDaysLabel->Invalidate();
					NoticeChange();
				}
				break;
			}
			case BM_PWD_STORED_CHANGED: {
				bool val = mStorePwdControl->Value();
				mPwdControl->SetEnabled( val);
				if (!val)
					mPwdControl->SetText("");
				if (mCurrAcc)
					mCurrAcc->PwdStoredOnDisk( val);
				NoticeChange();
				break;
			}
			case BM_CHECK_IF_PPP_UP_CHANGED: {
				ThePrefs->SetBool( "AutoCheckOnlyIfPPPRunning", 
										 mAutoCheckIfPppUpControl->Value());
				NoticeChange();
				break;
			}
			case BM_ENCRYPTION_SELECTED: {
				EncryptionSelected();
				NoticeChange();
				break;
			}
			case BM_AUTH_SELECTED: {
				AuthTypeSelected();
				NoticeChange();
				break;
			}
			case BM_HOME_FOLDER_SELECTED: {
				BView* srcView = NULL;
				msg->FindPointer( "source", (void**)&srcView);
				BMenuItem* item = dynamic_cast<BMenuItem*>( srcView);
				mHomeFolderControl->ClearMark();
				if (item) {
					BMenuItem* currItem = item;
					BMenu* currMenu = item->Menu();
					BmString path;
					while( currMenu && currItem 
					&& currItem!=mHomeFolderControl->MenuItem()) {
						if (!path.Length())
							path.Prepend( BmString(currItem->Label()));
						else
							path.Prepend( BmString(currItem->Label()) << "/");
						currItem = currMenu->Superitem();
						currMenu = currMenu->Supermenu();
					}
					mCurrAcc->HomeFolder( path);
					mHomeFolderControl->MarkItem( path.String());
				} else {
					mCurrAcc->HomeFolder( BmMailFolder::IN_FOLDER_NAME);
					mHomeFolderControl->MarkItem( BmMailFolder::IN_FOLDER_NAME);
				}
				NoticeChange();
				break;
			}
			case BM_FILTER_CHAIN_SELECTED: {
				BMenuItem* item = mFilterChainControl->Menu()->FindMarked();
				if (item && BM_NoItemLabel != item->Label())
					mCurrAcc->FilterChain( item->Label());
				else
					mCurrAcc->FilterChain( "");
				NoticeChange();
				break;
			}
			case BM_CHECK_AND_SUGGEST: {
				if (mCurrAcc) {
					// ToDo: this should be made asynchronous (using messages):
					BmString suggestedAuthType;
					BmString encryptionType;
					BmPopAccount* popAcc 
						= dynamic_cast<BmPopAccount*>(mCurrAcc.Get());
					if (popAcc) {
						BmRef<BmPopper> popper( new BmPopper( mCurrAcc->Key(), 
																		  popAcc));
						popper->StartJobInThisThread( 
							BmPopper::BM_CHECK_CAPABILITIES_JOB
						);
						suggestedAuthType = popper->SuggestAuthType();
						if (popper->SupportsTLS())
							// if server supports STARTTLS, we use it:
							encryptionType = BmPopAccount::ENCR_STARTTLS;
					} else {
						BmImapAccount* imapAcc 
							= dynamic_cast<BmImapAccount*>(mCurrAcc.Get());
						if (imapAcc) {
							BmRef<BmImap> imap( new BmImap( mCurrAcc->Key(), 
																	  imapAcc));
							imap->StartJobInThisThread( 
								BmImap::BM_CHECK_CAPABILITIES_JOB
							);
							suggestedAuthType = imap->SuggestAuthType();
							if (imap->SupportsTLS())
								// if server supports STARTTLS, we use it:
								encryptionType = BmImapAccount::ENCR_STARTTLS;
						}
					}
					if (suggestedAuthType.Length())
						mAuthControl->MarkItem( suggestedAuthType.String());
					else
						mAuthControl->MarkItem( BM_NoItemLabel.String());
					AuthTypeSelected();
					if (encryptionType.Length())
						mEncryptionControl->MarkItem( encryptionType.String());
					else
						mEncryptionControl->MarkItem( BM_NoItemLabel.String());
					NoticeChange();
				}
				break;
			}
			case BM_CLIENT_CERT_SELECTED: {
				entry_ref certRef;
				if (msg->FindRef( "refs", 0, &certRef) != B_OK) {
					// first step, let user select new certificate:
					if (!mClientCertPanel) {
						BmString certPath = TheNetEndpointRoster->GetCertPath();
						entry_ref eref;
						status_t err = get_ref_for_path(certPath.String(), &eref);
						mClientCertPanel = new BFilePanel( 
							B_OPEN_PANEL, new BMessenger(this), 
							err == B_OK ? &eref : NULL, B_FILE_NODE, false, msg
						);
					}
					mClientCertPanel->Show();
				} else {
					// second step, set data accordingly:
					mCurrAcc->ClientCertificate(certRef.name);
					mClientCertControl->SetText(certRef.name);
					NoticeChange();
				}
				break;
			}
			case BM_CLEAR_ACCEPTED_CERT: {
				mCurrAcc->AcceptedCertID("");
				mAcceptedCertControl->SetText("");
				NoticeChange();
				break;
			}
			case BM_ADD_POP_ACCOUNT:
			case BM_ADD_IMAP_ACCOUNT: {
				BmString key( "new account");
				for( int32 i=1; TheRecvAccountList->FindItemByKey( key); ++i) {
					key = BmString("new account_")<<i;
				}
				BmRecvAccount* recvAcc;
				if (msg->what == BM_ADD_POP_ACCOUNT)
					recvAcc = new BmPopAccount( key.String(), 
														 TheRecvAccountList.Get());
				else
					recvAcc = new BmImapAccount( key.String(), 
														  TheRecvAccountList.Get());
				TheRecvAccountList->AddItemToList( recvAcc);
				mAccountControl->MakeFocus( true);
				mAccountControl->TextView()->SelectAll();
				NoticeChange();
				// create an identity corresponding to this account:
				BmIdentity* newIdent 
					= new BmIdentity( key.String(), TheIdentityList.Get());
				newIdent->RecvAccount( key);
				TheIdentityList->AddItemToList( newIdent);
				break;
			}
			case BM_REMOVE_ACCOUNT: {
				int32 buttonPressed;
				if (msg->FindInt32( "which", &buttonPressed) != B_OK) {
					// first step, ask user about it:
					BAlert* alert = new BAlert( 
						"Remove Mail-Account", 
						(BmString("Are you sure about removing the account <") 
							<< mCurrAcc->Name() << ">?").String(),
						"Remove Account and its Identities", 
						"Remove Account, Keep Identities", 
						"Cancel",
						 B_WIDTH_AS_USUAL, B_WARNING_ALERT
					);
					alert->SetShortcut( 2, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(BM_REMOVE_ACCOUNT), 
													 BMessenger( this)));
				} else {
					// second step, do it if user said ok:
					if (buttonPressed < 2) {
						if (buttonPressed == 0) {
							BmIdentityVect identities;
							TheIdentityList->FindIdentitiesForRecvAccount( 
								mCurrAcc->Key(), identities);
							BmIdentityVect::iterator iter;
							for (iter = identities.begin(); iter != identities.end();
								++iter) {
								TheIdentityList->RemoveItemFromList( iter->Get());
							}
						}
						TheRecvAccountList->RemoveItemFromList( mCurrAcc.Get());
						mCurrAcc = NULL;
						NoticeChange();
					}
				}
				break;
			}
			case BM_COMPLAIN_ABOUT_FIELD: {
				int32 buttonPressed;
				if (msg->FindInt32( "which", &buttonPressed) != B_OK) {
					BmString complaint;
					complaint = msg->FindString( MSG_COMPLAINT);
					// first step, tell user about complaint:
					BAlert* alert = new BAlert( "Sanity Check Failed", 
														 complaint.String(),
													 	 "OK", NULL, NULL, B_WIDTH_AS_USUAL,
													 	 B_WARNING_ALERT);
					alert->SetShortcut( 0, B_ESCAPE);
					alert->Go( new BInvoker( new BMessage(*msg), BMessenger( this)));
					BmRecvAccount* acc=NULL;
					msg->FindPointer( MSG_ITEM, (void**)&acc);
					BmListViewItem* accItem = mAccListView->FindViewItemFor( acc);
					if (accItem)
						mAccListView->Select( mAccListView->IndexOf( accItem));
				} else {
					// second step, set corresponding focus:
					BmString fieldName;
					fieldName = msg->FindString( MSG_FIELD_NAME);
					if (fieldName.ICompare( "username")==0)
						mLoginControl->MakeFocus( true);
					else if (fieldName.ICompare( "recvserver")==0)
						mServerControl->MakeFocus( true);
					else if (fieldName.ICompare( "portnr")==0)
						mPortControl->MakeFocus( true);
					else if (fieldName.ICompare( "checkinterval")==0)
						mCheckIntervalControl->MakeFocus( true);
					else if (fieldName.ICompare( "pwdstoredondisk")==0)
						mStorePwdControl->MakeFocus( true);
					else if (fieldName.ICompare( "authmethod")==0)
						mAuthControl->MakeFocus( true);
				}
				break;
			}
			default:
				inherited::MessageReceived( msg);
		}
	}
	catch( BM_error &err) {
		// a problem occurred, we tell the user:
		BM_SHOWERR( BmString("PrefsView_") << Name() << ":\n\t" << err.what());
	}
}

/*------------------------------------------------------------------------------*\
	()
		-	
\*------------------------------------------------------------------------------*/
void BmPrefsRecvMailView::ShowAccount( int32 selection) {
	bool enabled = (selection != -1);
	mAccountControl->SetEnabled( enabled);
	mLoginControl->SetEnabled( enabled);
	mPortControl->SetEnabled( enabled);
	mServerControl->SetEnabled( enabled);
	mAcceptedCertControl->SetEnabled( false);
	if (TheNetEndpointRoster->SupportsEncryption()) {
		mEncryptionControl->SetEnabled( enabled);
		mClientCertControl->SetEnabled( enabled);
		mSelectClientCertButton->SetEnabled( enabled);
		mClearAcceptedCertButton->SetEnabled( enabled);
	} else {
		mEncryptionControl->SetEnabled( false);
		mClientCertControl->SetEnabled( false);
		mSelectClientCertButton->SetEnabled( false);
		mClearAcceptedCertButton->SetEnabled( false);
	}
	mAuthControl->SetEnabled( enabled);
	mHomeFolderControl->SetEnabled( enabled);
	mFilterChainControl->SetEnabled( enabled);
	mRemoveButton->SetEnabled( enabled);
	mCheckAccountControl->SetEnabled( enabled);
	mAutoCheckIfPppUpControl->SetEnabled( enabled);
	mCheckEveryControl->SetEnabled( enabled);
	mCheckAndSuggestButton->SetEnabled( enabled);
	mRemoveMailControl->SetEnabled( enabled);
	mStorePwdControl->SetEnabled( enabled);
	
	mAuthControl->MakeEmpty();

	if (selection == -1) {
		mCurrAcc = NULL;
		mAccountControl->SetTextSilently( "");
		mLoginControl->SetTextSilently( "");
		mPortControl->SetTextSilently( "");
		mPwdControl->SetTextSilently( "");
		mServerControl->SetTextSilently( "");
		mCheckIntervalControl->SetTextSilently( "");
		mEncryptionControl->ClearMark();
		mAuthControl->ClearMark();
		mHomeFolderControl->ClearMark();
		mFilterChainControl->ClearMark();
		mCheckAccountControl->SetValue( 0);
		mAutoCheckIfPppUpControl->SetValue( 0);
		mCheckEveryControl->SetValue( 0);
		mRemoveMailControl->SetValue( 0);
		mStorePwdControl->SetValue( 0);
		mPwdControl->SetEnabled( false);
		mCheckIntervalControl->SetEnabled( false);
		mDeleteMailDelayControl->SetEnabled( false);
		mMinutesLabel->SetHighColor( 
			tint_color( ui_color( B_PANEL_TEXT_COLOR), B_DISABLED_MARK_TINT)
		);
		mDaysLabel->SetHighColor(
			tint_color( ui_color( B_PANEL_TEXT_COLOR), B_DISABLED_MARK_TINT)
		);
		mClientCertControl->SetTextSilently( "");
		mAcceptedCertControl->SetTextSilently( "");
	} else {
		BmRecvAccItem* accItem 
			= dynamic_cast<BmRecvAccItem*>(mAccListView->ItemAt( selection));
		if (accItem) {
			if  (mCurrAcc != accItem->ModelItem()) {
				mCurrAcc = accItem->ModelItem();
				if (mCurrAcc) {
					mAccountControl->SetTextSilently( 
						mCurrAcc->Name().String());
					mLoginControl->SetTextSilently( 
						mCurrAcc->Username().String());
					mPortControl->SetTextSilently( 
						mCurrAcc->PortNrString().String());
					mPwdControl->SetTextSilently( 
						mCurrAcc->Password().String());
					mServerControl->SetTextSilently( 
						mCurrAcc->Server().String());
					mEncryptionControl->MarkItem( 
						(mCurrAcc->EncryptionType().Length() 
						&& TheNetEndpointRoster->SupportsEncryption())
								? mCurrAcc->EncryptionType().String()
								: BM_NoItemLabel.String()
					);
					vector<BmString> authTypes;
					mCurrAcc->GetSupportedAuthTypes(authTypes);
					vector<BmString>::iterator authTypeIter = authTypes.begin();
					for( ; authTypeIter != authTypes.end(); ++authTypeIter) {
						AddItemToMenu( 
							mAuthControl->Menu(), 
							new BMenuItem( authTypeIter->String(),
												new BMessage(BM_AUTH_SELECTED)),
							this
						);
					}
					mAuthControl->MarkItem( mCurrAcc->AuthMethod().String());
					mHomeFolderControl->MarkItem( 
						mCurrAcc->HomeFolder().String());
					mFilterChainControl->MarkItem( 
						mCurrAcc->FilterChain().Length() 
							? mCurrAcc->FilterChain().String()
							: BM_NoItemLabel.String()
					);
					mCheckAccountControl->SetValue( mCurrAcc->CheckMail());
					mAutoCheckIfPppUpControl->SetValueSilently( 
						ThePrefs->GetBool("AutoCheckOnlyIfPPPRunning")
					);
					mCheckEveryControl->SetValue( 
						mCurrAcc->CheckInterval()>0 ? 1 : 0
					);
					mCheckIntervalControl->SetTextSilently( 
						mCurrAcc->CheckIntervalString().String()
					);
					mRemoveMailControl->SetValue( 
						mCurrAcc->DeleteMailFromServer()
					);
					mDeleteMailDelayControl->SetTextSilently( 
						mCurrAcc->DeleteMailDelayString().String()
					);
					mStorePwdControl->SetValue( 
						mCurrAcc->PwdStoredOnDisk()
					);
					mPwdControl->SetEnabled( 
						mCurrAcc->PwdStoredOnDisk()
					);
					mCheckIntervalControl->SetEnabled( 
						mCurrAcc->CheckInterval()>0
					);
					mMinutesLabel->SetHighColor( 
						tint_color( 
							ui_color( B_PANEL_TEXT_COLOR),
							mCurrAcc->CheckInterval()>0 
								? B_NO_TINT 
								: B_DISABLED_MARK_TINT
						)
					);
					mDeleteMailDelayControl->SetEnabled( 
						mCurrAcc->DeleteMailFromServer()
					);
					mDaysLabel->SetHighColor( 
						tint_color( 
							ui_color( B_PANEL_TEXT_COLOR),
							mCurrAcc->DeleteMailFromServer()
								? B_NO_TINT 
								: B_DISABLED_MARK_TINT
						)
					);
					mClientCertControl->SetTextSilently( 
						mCurrAcc->ClientCertificate().String()
					);
					mAcceptedCertControl->SetTextSilently( 
						mCurrAcc->AcceptedCertID().String()
					);
				}
			}
		} else
			mCurrAcc = NULL;
	}
	mMinutesLabel->Invalidate();
	mDaysLabel->Invalidate();
}
