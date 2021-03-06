///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct  8 2012)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "dialog_choose_component_base.h"

///////////////////////////////////////////////////////////////////////////

DIALOG_CHOOSE_COMPONENT_BASE::DIALOG_CHOOSE_COMPONENT_BASE( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : DIALOG_SHIM( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 450,100 ), wxDefaultSize );
	
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );
	
	wxBoxSizer* bSearchSizer;
	bSearchSizer = new wxBoxSizer( wxHORIZONTAL );
	
	m_searchLabel = new wxStaticText( this, wxID_ANY, wxT("Search"), wxDefaultPosition, wxDefaultSize, 0 );
	m_searchLabel->Wrap( -1 );
	bSearchSizer->Add( m_searchLabel, 0, wxALL, 5 );
	
	m_searchBox = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
	bSearchSizer->Add( m_searchBox, 1, wxALL, 5 );
	
	
	bSizer1->Add( bSearchSizer, 0, wxEXPAND, 5 );
	
	m_libraryComponentTree = new wxTreeCtrl( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_DEFAULT_STYLE|wxTR_HIDE_ROOT );
	m_libraryComponentTree->SetMinSize( wxSize( -1,50 ) );
	
	bSizer1->Add( m_libraryComponentTree, 2, wxALL|wxEXPAND, 5 );
	
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );
	
	m_componentView = new SCH_COMPONENT_PREVIEW_PANEL( this ); 
	bSizer3->Add( m_componentView, 1, wxALL|wxEXPAND, 5 );
	
	m_componentDetails = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( -1,-1 ), wxTE_MULTILINE );
	m_componentDetails->SetMinSize( wxSize( -1,100 ) );
	
	bSizer3->Add( m_componentDetails, 2, wxALL|wxEXPAND, 5 );
	
	
	bSizer1->Add( bSizer3, 1, wxEXPAND, 5 );
	
	wxBoxSizer* bSizer5;
	bSizer5 = new wxBoxSizer( wxVERTICAL );
	
	m_button = new wxStdDialogButtonSizer();
	m_buttonOK = new wxButton( this, wxID_OK );
	m_button->AddButton( m_buttonOK );
	m_buttonCancel = new wxButton( this, wxID_CANCEL );
	m_button->AddButton( m_buttonCancel );
	m_button->Realize();
	
	bSizer5->Add( m_button, 0, wxEXPAND, 5 );
	
	
	bSizer1->Add( bSizer5, 0, wxALIGN_RIGHT, 5 );
	
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
	
	// Connect Events
	m_searchBox->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnInterceptSearchBoxKey ), NULL, this );
	m_searchBox->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnSearchBoxChange ), NULL, this );
	m_searchBox->Connect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnSearchBoxEnter ), NULL, this );
	m_libraryComponentTree->Connect( wxEVT_LEFT_UP, wxMouseEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnTreeMouseUp ), NULL, this );
	m_libraryComponentTree->Connect( wxEVT_COMMAND_TREE_ITEM_ACTIVATED, wxTreeEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnDoubleClickTreeSelect ), NULL, this );
	m_libraryComponentTree->Connect( wxEVT_COMMAND_TREE_SEL_CHANGED, wxTreeEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnTreeSelect ), NULL, this );
}

DIALOG_CHOOSE_COMPONENT_BASE::~DIALOG_CHOOSE_COMPONENT_BASE()
{
	// Disconnect Events
	m_searchBox->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnInterceptSearchBoxKey ), NULL, this );
	m_searchBox->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnSearchBoxChange ), NULL, this );
	m_searchBox->Disconnect( wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnSearchBoxEnter ), NULL, this );
	m_libraryComponentTree->Disconnect( wxEVT_LEFT_UP, wxMouseEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnTreeMouseUp ), NULL, this );
	m_libraryComponentTree->Disconnect( wxEVT_COMMAND_TREE_ITEM_ACTIVATED, wxTreeEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnDoubleClickTreeSelect ), NULL, this );
	m_libraryComponentTree->Disconnect( wxEVT_COMMAND_TREE_SEL_CHANGED, wxTreeEventHandler( DIALOG_CHOOSE_COMPONENT_BASE::OnTreeSelect ), NULL, this );
	
}
