/**
 * @file kicad/mainframe.cpp
 * @brief KICAD_MANAGER_FRAME is the KiCad main frame.
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 2013 CERN (www.cern.ch)
 * Copyright (C) 2004-2012 KiCad Developers, see change_log.txt for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <fctsys.h>
#include <appl_wxstruct.h>
#include <confirm.h>
#include <gestfich.h>
#include <macros.h>

#include <kicad.h>
#include <tree_project_frame.h>
#include <wildcards_and_files_ext.h>
#include <menus_helpers.h>


static const wxString TreeFrameWidthEntry( wxT( "LeftWinWidth" ) );

KICAD_MANAGER_FRAME::KICAD_MANAGER_FRAME( wxWindow*       parent,
                                          const wxString& title,
                                          const wxPoint&  pos,
                                          const wxSize&   size ) :
    EDA_BASE_FRAME( parent, KICAD_MAIN_FRAME_TYPE, title, pos, size,
                    KICAD_DEFAULT_DRAWFRAME_STYLE, wxT( "KicadFrame" ) )
{
    m_leftWinWidth = 60;

    // Create the status line (bottom of the frame
    static const int dims[3] = { -1, -1, 100 };

    CreateStatusBar( 3 );
    SetStatusWidths( 3, dims );

    // Give an icon
    wxIcon icon;
    icon.CopyFromBitmap( KiBitmap( icon_kicad_xpm ) );
    SetIcon( icon );

    // Give the last sise and pos to main window
    LoadSettings();
    SetSize( m_FramePos.x, m_FramePos.y, m_FrameSize.x, m_FrameSize.y );

    // Left window: is the box which display tree project
    m_LeftWin = new TREE_PROJECT_FRAME( this );

    // Right top Window: buttons to launch applications
    m_Launcher = new LAUNCHER_PANEL( this );
    // Add the wxTextCtrl showing all messages from KiCad:
    m_MessagesBox = new wxTextCtrl( this, wxID_ANY, wxEmptyString,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxTE_MULTILINE | wxSUNKEN_BORDER | wxTE_READONLY );

    RecreateBaseHToolbar();
    ReCreateMenuBar();

    m_auimgr.SetManagedWindow( this );

    EDA_PANEINFO horiztb;
    horiztb.HorizontalToolbarPane();

    EDA_PANEINFO info;
    info.InfoToolbarPane();

    m_auimgr.AddPane( m_mainToolBar,
                      wxAuiPaneInfo( horiztb ).Name( wxT( "m_mainToolBar" ) ).Top() );

    m_auimgr.AddPane( m_LeftWin,
                      wxAuiPaneInfo(info).Name( wxT( "m_LeftWin" ) ).Left().
                      BestSize( m_leftWinWidth, -1 ).
                      Layer( 1 ) );

    m_auimgr.AddPane( m_Launcher, wxTOP );
    m_auimgr.GetPane( m_Launcher).CaptionVisible( false ).Row(1)
        .BestSize( -1, m_Launcher->GetPanelHeight() ).PaneBorder( false ).Resizable( false );

    m_auimgr.AddPane( m_MessagesBox,
                      wxAuiPaneInfo().Name( wxT( "m_MessagesBox" ) ).CentrePane().Layer( 2 ) );

    m_auimgr.GetPane( m_LeftWin ).MinSize( wxSize( 80, -1) );
    m_auimgr.GetPane( m_LeftWin ).BestSize(wxSize(m_leftWinWidth, -1) );

    m_auimgr.Update();
}


KICAD_MANAGER_FRAME::~KICAD_MANAGER_FRAME()
{
    m_auimgr.UnInit();
}


void KICAD_MANAGER_FRAME::PrintMsg( const wxString& aText )
{
    m_MessagesBox->AppendText( aText );
}


void KICAD_MANAGER_FRAME::OnSize( wxSizeEvent& event )
{
    if( m_auimgr.GetManagedWindow() )
        m_auimgr.Update();

    event.Skip();
}


void KICAD_MANAGER_FRAME::OnCloseWindow( wxCloseEvent& Event )
{
    int px, py;

    UpdateFileHistory( m_ProjectFileName.GetFullPath() );

    if( !IsIconized() )   // save main frame position and size
    {
        GetPosition( &px, &py );
        m_FramePos.x = px;
        m_FramePos.y = py;

        GetSize( &px, &py );
        m_FrameSize.x = px;
        m_FrameSize.y = py;
    }

    Event.SetCanVeto( true );

    // Close the help frame
    if( wxGetApp().GetHtmlHelpController() )
    {
        if( wxGetApp().GetHtmlHelpController()->GetFrame() ) // returns NULL if no help frame active
            wxGetApp().GetHtmlHelpController()->GetFrame()->Close( true );

        wxGetApp().SetHtmlHelpController( NULL );
    }

    m_LeftWin->Show( false );

    Destroy();
}


void KICAD_MANAGER_FRAME::OnExit( wxCommandEvent& event )
{
    Close( true );
}


void KICAD_MANAGER_FRAME::PROCESS_TERMINATE_EVENT_HANDLER::
                        OnTerminate( int pid, int status )
{

    wxString msg;

    msg.Printf( appName + _( " closed [pid=%d]\n" ), pid );
    ( (KICAD_MANAGER_FRAME*) wxGetApp().GetTopWindow() )->PrintMsg( msg );

    delete this;
}


void KICAD_MANAGER_FRAME::Execute( wxWindow* frame, const wxString& execFile,
                                   const wxString& param )
{

    PROCESS_TERMINATE_EVENT_HANDLER* callback;
    long pid;
    wxString msg;

    callback = new PROCESS_TERMINATE_EVENT_HANDLER( execFile );
    pid = ExecuteFile( frame, execFile, param, callback );

    if( pid > 0 )
    {
        msg.Printf( execFile + _( " opened [pid=%ld]\n" ), pid );
        PrintMsg( msg );
    }
    else
    {
        delete callback;
    }
}


void KICAD_MANAGER_FRAME::OnRunBitmapConverter( wxCommandEvent& event )
{
    Execute( this, BITMAPCONVERTER_EXE );
}


void KICAD_MANAGER_FRAME::OnRunPcbCalculator( wxCommandEvent& event )
{
    Execute( this, PCB_CALCULATOR_EXE );
}

void KICAD_MANAGER_FRAME::OnRunPageLayoutEditor( wxCommandEvent& event )
{
    Execute( this, PL_EDITOR_EXE );
}


void KICAD_MANAGER_FRAME::OnRunPcbNew( wxCommandEvent& event )
{
    wxFileName  legacy_board( m_ProjectFileName );
    wxFileName  kicad_board( m_ProjectFileName );

    legacy_board.SetExt( LegacyPcbFileExtension );
    kicad_board.SetExt( KiCadPcbFileExtension );

    if( !legacy_board.FileExists() || kicad_board.FileExists() )
        Execute( this, PCBNEW_EXE, QuoteFullPath( kicad_board ) );
    else
        Execute( this, PCBNEW_EXE, QuoteFullPath( legacy_board ) );
}


void KICAD_MANAGER_FRAME::OnRunCvpcb( wxCommandEvent& event )
{
    wxFileName fn( m_ProjectFileName );

    fn.SetExt( NetlistFileExtension );
    Execute( this, CVPCB_EXE, QuoteFullPath( fn ) );
}

void KICAD_MANAGER_FRAME::OnRunEeschema( wxCommandEvent& event )
{
    wxFileName fn( m_ProjectFileName );

    fn.SetExt( SchematicFileExtension );
    Execute( this, EESCHEMA_EXE, QuoteFullPath( fn ) );

}

void KICAD_MANAGER_FRAME::OnRunGerbview( wxCommandEvent& event )
{
    wxFileName fn( m_ProjectFileName );
    wxString path = wxT( "\"" );
    path += fn.GetPath( wxPATH_GET_SEPARATOR | wxPATH_GET_VOLUME ) + wxT( "\"" );

    Execute( this, GERBVIEW_EXE, path );
}


void KICAD_MANAGER_FRAME::OnOpenTextEditor( wxCommandEvent& event )
{
    wxString editorname = wxGetApp().GetEditorName();

    if( !editorname.IsEmpty() )
        Execute( this, editorname, wxEmptyString );
}


void KICAD_MANAGER_FRAME::OnOpenFileInTextEditor( wxCommandEvent& event )
{
    wxString mask( wxT( "*" ) );

#ifdef __WINDOWS__
    mask += wxT( ".*" );
#endif

    mask = _( "Text file (" ) + mask + wxT( ")|" ) + mask;
    wxString default_dir = wxGetCwd();

    wxFileDialog dlg( this, _( "Load File to Edit" ), default_dir,
                      wxEmptyString, mask, wxFD_OPEN );

    if( dlg.ShowModal() == wxID_CANCEL )
        return;

    wxString filename = wxT( "\"" );
    filename += dlg.GetPath() + wxT( "\"" );

    if( !dlg.GetPath().IsEmpty() &&  !wxGetApp().GetEditorName().IsEmpty() )
        Execute( this, wxGetApp().GetEditorName(), filename );
}


void KICAD_MANAGER_FRAME::OnRefresh( wxCommandEvent& event )
{
    m_LeftWin->ReCreateTreePrj();
}


void KICAD_MANAGER_FRAME::ClearMsg()
{
    m_MessagesBox->Clear();
}


void KICAD_MANAGER_FRAME::LoadSettings()
{
    wxConfig* cfg = wxGetApp().GetSettings();

    if( cfg )
    {
        EDA_BASE_FRAME::LoadSettings();
        cfg->Read( TreeFrameWidthEntry, &m_leftWinWidth );
    }
}


void KICAD_MANAGER_FRAME::SaveSettings()
{
    wxConfig* cfg = wxGetApp().GetSettings();

    if( cfg )
    {
        EDA_BASE_FRAME::SaveSettings();
        cfg->Write( TreeFrameWidthEntry, m_LeftWin->GetSize().x );
    }
}

/**
 * a minor helper function:
 * Prints the Current Working Dir name and the projet name on the text panel.
 */
void KICAD_MANAGER_FRAME::PrintPrjInfo()
{
    wxString msg;
    msg.Printf( _( "Working dir: %s\nProject: %s\n" ),
                GetChars( wxGetCwd() ),
                GetChars( m_ProjectFileName.GetFullPath() ) );
    PrintMsg( msg );
}
