/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2015-2017 KiCad Developers, see change_log.txt for contributors.
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

#include <base_units.h>
#include <kiway.h>
#include <sch_draw_panel.h>
#include <sch_view.h>
#include <sch_painter.h>
#include <confirm.h>

#include <class_library.h>
#include <eeschema_id.h>
#include <lib_edit_frame.h>
#include <viewlib_frame.h>
#include <sch_base_frame.h>
#include <symbol_lib_table.h>
#include <pgm_base.h>
#include <sch_view.h>
#include "dialogs/dialog_sym_lib_table.h"

#include <gal/graphics_abstraction_layer.h>

LIB_ALIAS* SchGetLibAlias( const LIB_ID& aLibId, SYMBOL_LIB_TABLE* aLibTable, PART_LIB* aCacheLib,
                           wxWindow* aParent, bool aShowErrorMsg )
{
    // wxCHECK_MSG( aLibId.IsValid(), NULL, "LIB_ID is not valid." );
    wxCHECK_MSG( aLibTable, NULL, "Invalid symbol library table." );

    LIB_ALIAS* alias = NULL;

    try
    {
        alias = aLibTable->LoadSymbol( aLibId );

        if( !alias && aCacheLib )
            alias = aCacheLib->FindAlias( aLibId );
    }
    catch( const IO_ERROR& ioe )
    {
        if( aShowErrorMsg )
        {
            wxString msg;

            msg.Printf( _( "Could not load symbol \"%s\" from library \"%s\"." ),
                        aLibId.GetLibItemName().wx_str(), aLibId.GetLibNickname().wx_str() );
            DisplayErrorMessage( aParent, msg, ioe.What() );
        }
    }

    return alias;
}


LIB_PART* SchGetLibPart( const LIB_ID& aLibId, SYMBOL_LIB_TABLE* aLibTable, PART_LIB* aCacheLib,
                         wxWindow* aParent, bool aShowErrorMsg )
{
    LIB_ALIAS* alias = SchGetLibAlias( aLibId, aLibTable, aCacheLib, aParent, aShowErrorMsg );

    return ( alias ) ? alias->GetPart() : NULL;
}


// Sttaic members:

SCH_BASE_FRAME::SCH_BASE_FRAME( KIWAY* aKiway, wxWindow* aParent,
        FRAME_T aWindowType, const wxString& aTitle,
        const wxPoint& aPosition, const wxSize& aSize, long aStyle,
        const wxString& aFrameName ) :
    EDA_DRAW_FRAME( aKiway, aParent, aWindowType, aTitle, aPosition,
            aSize, aStyle, aFrameName )
{
    m_zoomLevelCoeff = 11.0;    // Adjusted to roughly displays zoom level = 1
                                // when the screen shows a 1:1 image
                                // obviously depends on the monitor,
                                // but this is an acceptable value
    m_repeatStep = wxPoint( DEFAULT_REPEAT_OFFSET_X, DEFAULT_REPEAT_OFFSET_Y );
    m_repeatDeltaLabel = DEFAULT_REPEAT_LABEL_INC;
}



SCH_BASE_FRAME::~SCH_BASE_FRAME()
{
}


void SCH_BASE_FRAME::OnOpenLibraryViewer( wxCommandEvent& event )
{
    LIB_VIEW_FRAME* viewlibFrame = (LIB_VIEW_FRAME*) Kiway().Player( FRAME_SCH_VIEWER, true );

    viewlibFrame->PushPreferences( m_canvas );

    // On Windows, Raise() does not bring the window on screen, when iconized
    if( viewlibFrame->IsIconized() )
        viewlibFrame->Iconize( false );

    viewlibFrame->Show( true );
    viewlibFrame->Raise();
}


// Virtual from EDA_DRAW_FRAME
COLOR4D SCH_BASE_FRAME::GetDrawBgColor() const
{
    return GetLayerColor( LAYER_SCHEMATIC_BACKGROUND );
}


void SCH_BASE_FRAME::SetDrawBgColor( COLOR4D aColor )
{
    m_drawBgColor= aColor;
    SetLayerColor( aColor, LAYER_SCHEMATIC_BACKGROUND );
}


SCH_SCREEN* SCH_BASE_FRAME::GetScreen() const
{
    return (SCH_SCREEN*) EDA_DRAW_FRAME::GetScreen();
}


const wxString SCH_BASE_FRAME::GetZoomLevelIndicator() const
{
    return EDA_DRAW_FRAME::GetZoomLevelIndicator();
}


void SCH_BASE_FRAME::SetPageSettings( const PAGE_INFO& aPageSettings )
{
    GetScreen()->SetPageSettings( aPageSettings );
}


const PAGE_INFO& SCH_BASE_FRAME::GetPageSettings () const
{
    return GetScreen()->GetPageSettings();
}


const wxSize SCH_BASE_FRAME::GetPageSizeIU() const
{
    // GetSizeIU is compile time dependent:
    return GetScreen()->GetPageSettings().GetSizeIU();
}


const wxPoint& SCH_BASE_FRAME::GetAuxOrigin() const
{
    wxASSERT( GetScreen() );
    return GetScreen()->GetAuxOrigin();
}


void SCH_BASE_FRAME::SetAuxOrigin( const wxPoint& aPosition )
{
    wxASSERT( GetScreen() );
    GetScreen()->SetAuxOrigin( aPosition );
}


const TITLE_BLOCK& SCH_BASE_FRAME::GetTitleBlock() const
{
    wxASSERT( GetScreen() );
    return GetScreen()->GetTitleBlock();
}


void SCH_BASE_FRAME::SetTitleBlock( const TITLE_BLOCK& aTitleBlock )
{
    wxASSERT( GetScreen() );
    GetScreen()->SetTitleBlock( aTitleBlock );
}


void SCH_BASE_FRAME::UpdateStatusBar()
{
    wxString        line;
    int             dx, dy;
    BASE_SCREEN*    screen = GetScreen();

    if( !screen )
        return;

    EDA_DRAW_FRAME::UpdateStatusBar();

    // Display absolute coordinates:
    double dXpos = To_User_Unit( g_UserUnit, GetCrossHairPosition().x );
    double dYpos = To_User_Unit( g_UserUnit, GetCrossHairPosition().y );

    if ( g_UserUnit == MILLIMETRES )
    {
        dXpos = RoundTo0( dXpos, 100.0 );
        dYpos = RoundTo0( dYpos, 100.0 );
    }

    wxString absformatter;
    wxString locformatter;

    switch( g_UserUnit )
    {
    case INCHES:
        absformatter = wxT( "X %.3f  Y %.3f" );
        locformatter = wxT( "dx %.3f  dy %.3f  dist %.3f" );
        break;

    case MILLIMETRES:
        absformatter = wxT( "X %.2f  Y %.2f" );
        locformatter = wxT( "dx %.2f  dy %.2f  dist %.2f" );
        break;

    case UNSCALED_UNITS:
        absformatter = wxT( "X %f  Y %f" );
        locformatter = wxT( "dx %f  dy %f  dist %f" );
        break;

    case DEGREES:
        wxASSERT( false );
        break;
    }

    line.Printf( absformatter, dXpos, dYpos );
    SetStatusText( line, 2 );

    // Display relative coordinates:
    dx = GetCrossHairPosition().x - screen->m_O_Curseur.x;
    dy = GetCrossHairPosition().y - screen->m_O_Curseur.y;

    dXpos = To_User_Unit( g_UserUnit, dx );
    dYpos = To_User_Unit( g_UserUnit, dy );

    if( g_UserUnit == MILLIMETRES )
    {
        dXpos = RoundTo0( dXpos, 100.0 );
        dYpos = RoundTo0( dYpos, 100.0 );
    }

    // We already decided the formatter above
    line.Printf( locformatter, dXpos, dYpos, hypot( dXpos, dYpos ) );
    SetStatusText( line, 3 );

    // refresh units display
    DisplayUnitsMsg();
}


void SCH_BASE_FRAME::OnConfigurePaths( wxCommandEvent& aEvent )
{
    Pgm().ConfigurePaths( this );
}


void SCH_BASE_FRAME::OnEditSymbolLibTable( wxCommandEvent& aEvent )
{
    DIALOG_SYMBOL_LIB_TABLE dlg( this, &SYMBOL_LIB_TABLE::GetGlobalLibTable(),
                                 Prj().SchSymbolLibTable() );

    if( dlg.ShowModal() == wxID_CANCEL )
        return;

    saveSymbolLibTables( true, true );

    LIB_EDIT_FRAME* editor = (LIB_EDIT_FRAME*) Kiway().Player( FRAME_SCH_LIB_EDITOR, false );

    if( this == editor )
    {
        // There may be no parent window so use KIWAY message to refresh the schematic editor
        // in case any symbols have changed.
        Kiway().ExpressMail( FRAME_SCH, MAIL_SCH_REFRESH, std::string( "" ), this );
    }

    LIB_VIEW_FRAME* viewer = (LIB_VIEW_FRAME*) Kiway().Player( FRAME_SCH_VIEWER, false );

    if( viewer )
        viewer->ReCreateListLib();
}


LIB_ALIAS* SCH_BASE_FRAME::GetLibAlias( const LIB_ID& aLibId, bool aUseCacheLib,
                                        bool aShowErrorMsg )
{
    // wxCHECK_MSG( aLibId.IsValid(), NULL, "LIB_ID is not valid." );

    PART_LIB* cache = ( aUseCacheLib ) ? Prj().SchLibs()->GetCacheLibrary() : NULL;

    return SchGetLibAlias( aLibId, Prj().SchSymbolLibTable(), cache, this, aShowErrorMsg );
}


LIB_PART* SCH_BASE_FRAME::GetLibPart( const LIB_ID& aLibId, bool aUseCacheLib, bool aShowErrorMsg )
{
    // wxCHECK_MSG( aLibId.IsValid(), NULL, "LIB_ID is not valid." );

    PART_LIB* cache = ( aUseCacheLib ) ? Prj().SchLibs()->GetCacheLibrary() : NULL;

    return SchGetLibPart( aLibId, Prj().SchSymbolLibTable(), cache, this, aShowErrorMsg );
}


bool SCH_BASE_FRAME::saveSymbolLibTables( bool aGlobal, bool aProject )
{
    bool success = true;

    if( aGlobal )
    {
        try
        {
            FILE_OUTPUTFORMATTER sf( SYMBOL_LIB_TABLE::GetGlobalTableFileName() );

            SYMBOL_LIB_TABLE::GetGlobalLibTable().Format( &sf, 0 );
        }
        catch( const IO_ERROR& ioe )
        {
            success = false;
            wxString msg = wxString::Format( _( "Error occurred saving the global symbol library "
                                                "table:\n\n%s" ),
                                            GetChars( ioe.What().GetData() ) );
            wxMessageBox( msg, _( "File Save Error" ), wxOK | wxICON_ERROR );
        }
    }

    if( aProject && !Prj().GetProjectName().IsEmpty() )
    {
        wxFileName fn( Prj().GetProjectPath(), SYMBOL_LIB_TABLE::GetSymbolLibTableFileName() );

        try
        {
            Prj().SchSymbolLibTable()->Save( fn.GetFullPath() );
        }
        catch( const IO_ERROR& ioe )
        {
            success = false;
            wxString msg = wxString::Format( _( "Error occurred saving project specific "
                                                "symbol library table:\n\n%s" ),
                                             GetChars( ioe.What() ) );
            wxMessageBox( msg, _( "File Save Error" ), wxOK | wxICON_ERROR );
        }
    }

    return success;
}

void SCH_BASE_FRAME::Zoom_Automatique( bool aWarpPointer )
{
    auto galCanvas = GetGalCanvas();
    auto view = GetGalCanvas()->GetView();

    BOX2I bBox = GetDocumentExtents();

    VECTOR2D scrollbarSize = VECTOR2D( galCanvas->GetSize() - galCanvas->GetClientSize() );
    VECTOR2D screenSize = view->ToWorld( galCanvas->GetClientSize(), false );

    if( bBox.GetWidth() == 0 || bBox.GetHeight() == 0 )
    {
        bBox = galCanvas->GetDefaultViewBBox();
    }

    VECTOR2D vsize = bBox.GetSize();
    double scale = view->GetScale() / std::max( fabs( vsize.x / screenSize.x ),
                                                fabs( vsize.y / screenSize.y ) );

    // Reserve a 10% margin around component bounding box.
    double margin_scale_factor = 1.1;

    // Leave 20% for library editors & viewers
    if( IsType( FRAME_PCB_MODULE_VIEWER ) || IsType( FRAME_PCB_MODULE_VIEWER_MODAL )
            || IsType( FRAME_SCH_VIEWER ) || IsType( FRAME_SCH_VIEWER_MODAL )
            || IsType( FRAME_SCH_LIB_EDITOR ) || IsType( FRAME_PCB_MODULE_EDITOR ) )
    {
        margin_scale_factor = 1.2;
    }

    view->SetScale( scale / margin_scale_factor );
    view->SetCenter( bBox.Centre() );

    // Take scrollbars into account
    VECTOR2D worldScrollbarSize = view->ToWorld( scrollbarSize, false );
    view->SetCenter( view->GetCenter() + worldScrollbarSize / 2.0 );
    galCanvas->Refresh();
}

                               /* Set the zoom level to show the area Rect */
void SCH_BASE_FRAME::Window_Zoom( EDA_RECT& Rect )
{
    auto view = GetGalCanvas()->GetView();
    BOX2I selectionBox ( Rect.GetPosition(), Rect.GetSize() );

    VECTOR2D screenSize = view->ToWorld( GetGalCanvas()->GetClientSize(), false );

    if( selectionBox.GetWidth() == 0 || selectionBox.GetHeight() == 0 )
        return;

    VECTOR2D vsize = selectionBox.GetSize();
    double scale;
    double ratio = std::max( fabs( vsize.x / screenSize.x ),
                             fabs( vsize.y / screenSize.y ) );

    scale = view->GetScale() / ratio;

    view->SetScale( scale );
    view->SetCenter( selectionBox.Centre() );
    GetGalCanvas()->Refresh();
}


void SCH_BASE_FRAME::RedrawScreen( const wxPoint& aCenterPoint, bool aWarpPointer )
{
    GetGalCanvas()->Refresh();
}


void SCH_BASE_FRAME::RedrawScreen2( const wxPoint& posBefore )
{
    GetGalCanvas()->Refresh();
}

SCH_DRAW_PANEL *SCH_BASE_FRAME::GetCanvas() const
{
    return static_cast<SCH_DRAW_PANEL*>( GetGalCanvas() );
}


bool SCH_BASE_FRAME::HandleBlockBegin( wxDC* aDC, EDA_KEY aKey, const wxPoint& aPosition,
       int aExplicitCommand )
{
    BLOCK_SELECTOR* block = &GetScreen()->m_BlockLocate;

    if( ( block->GetCommand() != BLOCK_IDLE ) || ( block->GetState() != STATE_NO_BLOCK ) )
        return false;

    if( aExplicitCommand == 0 )
        block->SetCommand( (BLOCK_COMMAND_T) BlockCommand( aKey ) );
    else
        block->SetCommand( (BLOCK_COMMAND_T) aExplicitCommand );

    if( block->GetCommand() == 0 )
        return false;

    switch( block->GetCommand() )
    {
    case BLOCK_IDLE:
        break;

    case BLOCK_MOVE:                // Move
    case BLOCK_DRAG:                // Drag (block defined)
    case BLOCK_DRAG_ITEM:           // Drag from a drag item command
    case BLOCK_DUPLICATE:           // Duplicate
    case BLOCK_DUPLICATE_AND_INCREMENT: // Duplicate and increment relevant references
    case BLOCK_DELETE:              // Delete
    case BLOCK_COPY:                // Copy
    case BLOCK_ROTATE:              // Rotate 90 deg
    case BLOCK_FLIP:                // Flip
    case BLOCK_ZOOM:                // Window Zoom
    case BLOCK_MIRROR_X:
    case BLOCK_MIRROR_Y:            // mirror
    case BLOCK_PRESELECT_MOVE:      // Move with preselection list
        block->InitData( m_canvas, aPosition );
        GetCanvas()->GetView()->ShowSelectionArea();
        break;

    case BLOCK_PASTE:
        block->InitData( m_canvas, aPosition );
        GetCanvas()->GetView()->ShowSelectionArea();
        block->SetLastCursorPosition( wxPoint( 0, 0 ) );
        InitBlockPasteInfos();

        if( block->GetCount() == 0 )      // No data to paste
        {
            DisplayError( this, wxT( "No block to paste" ), 20 );
            GetScreen()->m_BlockLocate.SetCommand( BLOCK_IDLE );
            m_canvas->SetMouseCaptureCallback( NULL );
            block->SetState( STATE_NO_BLOCK );
            block->SetMessageBlock( this );
            return true;
        }

        if( !m_canvas->IsMouseCaptured() )
        {
            block->ClearItemsList();
            DisplayError( this,
                          wxT( "EDA_DRAW_FRAME::HandleBlockBegin() Err: m_mouseCaptureCallback NULL" ) );
            block->SetState( STATE_NO_BLOCK );
            block->SetMessageBlock( this );
            return true;
        }

        block->SetState( STATE_BLOCK_MOVE );
        m_canvas->CallMouseCapture( aDC, aPosition, false );
        break;

    default:
        {
            wxString msg;
            msg << wxT( "EDA_DRAW_FRAME::HandleBlockBegin() error: Unknown command " ) <<
            block->GetCommand();
            DisplayError( this, msg );
        }
        break;
    }

    block->SetMessageBlock( this );
    return true;
}

void EDA_DRAW_FRAME::createCanvas()
{
    KIGFX::GAL_DISPLAY_OPTIONS options;

    m_canvas = new SCH_DRAW_PANEL( this, -1, wxPoint( 0,
                        0 ), m_FrameSize, options, EDA_DRAW_PANEL_GAL::GAL_TYPE_OPENGL );


    m_useSingleCanvasPane = true;

    SetGalCanvas( static_cast<SCH_DRAW_PANEL*> (m_canvas) );
    UseGalCanvas( true );
}


void SCH_BASE_FRAME::AddToScreen( SCH_ITEM* aItem )
{
        GetScreen()->Append( aItem );
        GetCanvas()->GetView()->Add( aItem );
}

void SCH_BASE_FRAME::AddToScreen( DLIST<SCH_ITEM>& aItems )
{
    std::vector<SCH_ITEM*> tmp;
        SCH_ITEM* itemList = aItems.begin();

        while( itemList )
        {
            itemList->SetList( nullptr );
            GetCanvas()->GetView()->Add( itemList );
            itemList = itemList->Next();
        }

        GetScreen()->Append( aItems );

}

void SCH_BASE_FRAME::RemoveFromScreen( SCH_ITEM* aItem )
{
    GetCanvas()->GetView()->Remove( aItem );
    GetScreen()->Remove( aItem );
}

void SCH_BASE_FRAME::SyncView()
{
    auto screen = GetScreen();
    auto gal = GetGalCanvas()->GetGAL();

    auto gs = screen->GetGridSize();

    gal->SetGridSize( VECTOR2D( gs.x, gs.y ));

    printf("SyncView: grid %d %d\n", (int)gs.x, (int)gs.y );

    GetGalCanvas()->GetView()->UpdateAllItems( KIGFX::ALL );
}
