/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013-2017 CERN
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
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

#include <wx/timer.h>
#include <wx/math.h>
#include <wx/log.h>
#include <wx/popupwin.h>
#include <wx/cmdline.h>

#include <layers_id_colors_and_visibility.h>

#include <gal/graphics_abstraction_layer.h>
#include <view/view.h>
#include <class_draw_panel_gal.h>
#include <pcb_draw_panel_gal.h>
#include <view/wx_view_controls.h>
#include <pcb_painter.h>

#include <class_pad.h>
#include <class_module.h>
#include <class_board.h>
#include <class_track.h>
#include <class_zone.h>

#include <pcb_painter.h>
#include <wxPcbStruct.h>

#include <connectivity.h>

#include <io_mgr.h>
#include <set>

#include <tool/actions.h>
#include <tool/tool_manager.h>
#include <tool/tool_dispatcher.h>
#include <tools/pcb_tool.h>
#include <tools/pcb_actions.h>
#include <tools/selection_tool.h>
#include <kicad_plugin.h>

#include "pcb_test_frame.h"

IMPLEMENT_APP( GAL_TEST_APP )

using namespace KIGFX;

bool GAL_TEST_APP::OnInit()
{
    if( !wxApp::OnInit() )
        return false;

    // Create the main frame window
    auto frame = CreateMainFrame( (const char*) m_filename.c_str() );

    return frame != nullptr;
}


void GAL_TEST_APP::OnInitCmdLine( wxCmdLineParser& parser )
{
    parser.AddOption( "f", wxEmptyString, "Open board file" );
    wxApp::OnInitCmdLine( parser );
}


bool GAL_TEST_APP::OnCmdLineParsed( wxCmdLineParser& parser )
{
    wxString filename;

    if( parser.Found( "f", &filename ) )
    {
        m_filename = filename;
    }

    return true;
}


class TEST_ACTIONS : public ACTIONS
{
public:

    virtual ~TEST_ACTIONS() {};

    virtual OPT<TOOL_EVENT> TranslateLegacyId( int aId )
    {
        return NULLOPT;
    }

    void RegisterAllTools( TOOL_MANAGER* aToolManager )
    {
    }
};

void PCB_TEST_FRAME::OnMenuFileOpen( wxCommandEvent& WXUNUSED( event ) )
{
}


void PCB_TEST_FRAME::OnMotion( wxMouseEvent& aEvent )
{
    aEvent.Skip();
}


void PCB_TEST_FRAME::SetBoard( BOARD* b )
{
    m_board.reset( b );
    m_board->GetConnectivity()->Build( m_board.get() );
    m_galPanel->DisplayBoard( m_board.get() );
    m_toolManager->SetEnvironment( m_board.get(), m_galPanel->GetView(),
            m_galPanel->GetViewControls(), nullptr );

    m_toolManager->ResetTools( TOOL_BASE::MODEL_RELOAD );
}


BOARD* PCB_TEST_FRAME::LoadAndDisplayBoard( const std::string& filename )
{
    PLUGIN::RELEASER pi( new PCB_IO );
    BOARD* brd = nullptr;

    try
    {
        brd = pi->Load( wxString( filename.c_str() ), NULL, NULL );
    }
    catch( const IO_ERROR& ioe )
    {
        wxString msg = wxString::Format( _( "Error loading board.\n%s" ),
                ioe.Problem() );

        printf( "%s\n", (const char*) msg.mb_str() );
        return nullptr;
    }

    SetBoard( brd );

    return brd;
}


PCB_TEST_FRAME::PCB_TEST_FRAME( wxFrame* frame, const wxString& title, const wxPoint& pos,
        const wxSize& size, long style ) :
    wxFrame( frame, wxID_ANY, title, pos, size, style )
{
    // Make a menubar
    wxMenu* fileMenu = new wxMenu;

    fileMenu->Append( wxID_OPEN, wxT( "&Open..." ) );
    fileMenu->AppendSeparator();
    fileMenu->Append( wxID_EXIT, wxT( "E&xit" ) );
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append( fileMenu, wxT( "&File" ) );
    SetMenuBar( menuBar );

    Show( true );
    Maximize();
    Raise();

    KIGFX::GAL_DISPLAY_OPTIONS options;

    m_galPanel.reset( new PCB_DRAW_PANEL_GAL( this, -1, wxPoint( 0,
                            0 ), wxDefaultSize, options, EDA_DRAW_PANEL_GAL::GAL_TYPE_OPENGL ) );

    m_galPanel->SetEvtHandlerEnabled( true );
    m_galPanel->SetFocus();
    m_galPanel->Show( true );
    m_galPanel->Raise();
    m_galPanel->StartDrawing();

    auto gal = m_galPanel->GetGAL();

    gal->SetGridVisibility( true );
    gal->SetGridSize( VECTOR2D( 100000.0, 100000.0 ) );
    gal->SetGridOrigin( VECTOR2D( 0.0, 0.0 ) );

    m_galPanel->Connect( wxEVT_MOTION,
            wxMouseEventHandler( PCB_TEST_FRAME::OnMotion ), NULL, this );

    m_galPanel->GetViewControls()->ShowCursor( true );

    m_toolManager.reset( new TOOL_MANAGER );
    m_toolManager->SetEnvironment( m_board.get(), m_galPanel->GetView(),
            m_galPanel->GetViewControls(), nullptr );

    m_pcbActions.reset( new TEST_ACTIONS() );
    m_toolDispatcher.reset( new TOOL_DISPATCHER( m_toolManager.get(), m_pcbActions.get() ) );

    m_toolManager->RegisterTool( new SELECTION_TOOL );

    m_toolManager->InitTools();
    m_galPanel->SetEventDispatcher( m_toolDispatcher.get() );
    m_toolManager->InvokeTool( "pcbnew.InteractiveSelection" );

    SetBoard( new BOARD );
}


PCB_TEST_FRAME::~PCB_TEST_FRAME()
{
}


// Intercept menu commands
void PCB_TEST_FRAME::OnExit( wxCommandEvent& WXUNUSED( event ) )
{
    // true is to force the frame to close
    Close( true );
}

void PCB_TEST_FRAME::AddMenuAction( wxMenu *where, const TOOL_ACTION* aAction )
{
    where->Append( m_lastActionId++, aAction->GetMenuItem() );
}

void PCB_TEST_FRAME::OnMenuAction( wxCommandEvent& event )
{

}
