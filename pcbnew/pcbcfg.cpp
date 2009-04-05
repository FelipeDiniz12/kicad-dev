/***********************************/
/** pcbcfg() : configuration	  **/
/***********************************/

/* lit ou met a jour la configuration de PCBNEW */

#include "fctsys.h"
#include "appl_wxstruct.h"
#include "common.h"
#include "class_drawpanel.h"
#include "confirm.h"
#include "gestfich.h"
#include "pcbnew.h"
#include "pcbplot.h"
#include "pcbcfg.h"
#include "worksheet.h"
#include "id.h"
#include "hotkeys.h"
#include "protos.h"

/* Routines Locales */

/* Variables locales */

#define HOTKEY_FILENAME wxT( "pcbnew" )

/***********************************************************/
void WinEDA_PcbFrame::Process_Config( wxCommandEvent& event )
/***********************************************************/
{
    int        id = event.GetId();
    wxPoint    pos;

    wxClientDC dc( DrawPanel );

    wxFileName fn;

    DrawPanel->PrepareGraphicContext( &dc );

    pos    = GetPosition();
    pos.x += 20;
    pos.y += 20;

    switch( id )
    {
    case ID_COLORS_SETUP:
        DisplayColorSetupFrame( this, pos );
        break;

    case ID_CONFIG_REQ:             // Creation de la fenetre de configuration
        InstallConfigFrame( pos );
        break;

    case ID_PCB_TRACK_SIZE_SETUP:
    case ID_PCB_LOOK_SETUP:
    case ID_OPTIONS_SETUP:
    case ID_PCB_DRAWINGS_WIDTHS_SETUP:
        InstallPcbOptionsFrame( pos, &dc, id );
        break;

    case ID_PCB_PAD_SETUP:
        InstallPadOptionsFrame( NULL, NULL, pos );
        break;

    case ID_CONFIG_SAVE:
        Update_config( this );
        break;

    case ID_CONFIG_READ:
    {
        fn = GetScreen()->m_FileName;
        fn.SetExt( ProjectFileExtension );

        wxFileDialog dlg( this, _( "Read Project File" ), fn.GetPath(),
                          fn.GetFullName(), ProjectFileWildcard,
                          wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_CHANGE_DIR );

        if( dlg.ShowModal() == wxID_CANCEL )
            break;

        if( !wxFileExists( dlg.GetPath() ) )
        {
            wxString msg;
            msg.Printf( _( "File %s not found" ), dlg.GetPath().c_str() );
            DisplayError( this, msg );
            break;
        }

        Read_Config( dlg.GetPath() );
        break;
    }
    case ID_PREFERENCES_CREATE_CONFIG_HOTKEYS:
        fn.SetPath( ReturnHotkeyConfigFilePath( g_ConfigFileLocationChoice ) );
        fn.SetName( HOTKEY_FILENAME );
        fn.SetExt( DEFAULT_HOTKEY_FILENAME_EXT );
        WriteHotkeyConfigFile( fn.GetFullPath(), s_Pcbnew_Editor_Hokeys_Descr,
                               true );
        break;

    case ID_PREFERENCES_READ_CONFIG_HOTKEYS:
        Read_Hotkey_Config( this, true );
        break;

    case ID_PREFERENCES_EDIT_CONFIG_HOTKEYS:
    {
        fn.SetPath( ReturnHotkeyConfigFilePath( g_ConfigFileLocationChoice ) );
        fn.SetName( HOTKEY_FILENAME );
        fn.SetExt( DEFAULT_HOTKEY_FILENAME_EXT );

        wxString editorname = wxGetApp().GetEditorName();
        if( !editorname.IsEmpty() )
            ExecuteFile( this, editorname, QuoteFullPath( fn ) );
        break;
    }

    case ID_PREFERENCES_HOTKEY_PATH_IS_HOME:
    case ID_PREFERENCES_HOTKEY_PATH_IS_KICAD:
        HandleHotkeyConfigMenuSelection( this, id );
        break;

    case ID_PREFERENCES_HOTKEY_SHOW_CURRENT_LIST:
        DisplayHotkeyList( this, s_Board_Editor_Hokeys_Descr );
        break;

    default:
        DisplayError( this,
                      wxT( "WinEDA_PcbFrame::Process_Config internal error" ) );
    }
}


/***************************************************************/
bool Read_Hotkey_Config( WinEDA_DrawFrame* frame, bool verbose )
/***************************************************************/

/*
 * Read the hotkey files config for pcbnew and module_edit
 */
{
    wxString FullFileName = ReturnHotkeyConfigFilePath(
        g_ConfigFileLocationChoice );

    FullFileName += HOTKEY_FILENAME;
    FullFileName += DEFAULT_HOTKEY_FILENAME_EXT;
    return frame->ReadHotkeyConfigFile( FullFileName,
                                        s_Pcbnew_Editor_Hokeys_Descr,
                                        verbose );
}


/**************************************************************************/
bool Read_Config( const wxString& projectFileName )
/*************************************************************************/

/* lit la configuration, si elle n'a pas deja ete lue
 * 1 - lit <nom fichier brd>.pro
 * 2 - si non trouve lit <chemin de *.exe>/kicad.pro
 * 3 - si non trouve: init des variables aux valeurs par defaut
 *
 * Retourne TRUE si lu, FALSE si config non lue ou non modifi�e
 */
{
    wxFileName fn = projectFileName;
    int      ii;

    if( fn.GetExt() != ProjectFileExtension )
    {
        wxLogDebug( wxT( "Attempting to open project file <%s>.  Changing " \
                         "file extension to a Kicad project file extension " \
                         "(.pro)." ), fn.GetFullPath().c_str() );
        fn.SetExt( ProjectFileExtension );
    }

    if( wxGetApp().GetLibraryPathList().Index( g_UserLibDirBuffer ) != wxNOT_FOUND )
    {
        wxLogDebug( wxT( "Removing path <%s> to library path search list." ),
                    g_UserLibDirBuffer.c_str() );
        wxGetApp().GetLibraryPathList().Remove( g_UserLibDirBuffer );
    }

    /* Init des valeurs par defaut */
    g_LibName_List.Clear();

    wxGetApp().ReadProjectConfig( fn.GetFullPath(),
                                  GROUP, ParamCfgList, FALSE );

    /* Traitement des variables particulieres: */

    if( wxFileName::DirExists( g_UserLibDirBuffer )
        && wxGetApp().GetLibraryPathList().Index( g_UserLibDirBuffer ) == wxNOT_FOUND )
    {
        wxLogDebug( wxT( "Adding path <%s> to library path search list." ),
                    g_UserLibDirBuffer.c_str() );
        wxGetApp().GetLibraryPathList().Add( g_UserLibDirBuffer );
    }

    g_DesignSettings.m_TrackWidthHistory[0] = g_DesignSettings.m_CurrentTrackWidth;
    g_DesignSettings.m_ViaSizeHistory[0]    = g_DesignSettings.m_CurrentViaSize;

    for( ii = 1; ii < HISTORY_NUMBER; ii++ )
    {
        g_DesignSettings.m_TrackWidthHistory[ii] = 0;
        g_DesignSettings.m_ViaSizeHistory[ii]    = 0;
    }

    return TRUE;
}


/**********************************************************/
void WinEDA_PcbFrame::Update_config( wxWindow* displayframe )
/***********************************************************/
/* enregistrement de la config */
{
    wxFileName fn;

    fn = GetScreen()->m_FileName;
    fn.SetExt( ProjectFileExtension );

    wxFileDialog dlg( this, _( "Save Project File" ), fn.GetPath(),
                      fn.GetFullName(), ProjectFileWildcard,
                      wxFD_SAVE | wxFD_CHANGE_DIR );

    if( dlg.ShowModal() == wxID_CANCEL )
        return;

    /* ecriture de la configuration */
    wxGetApp().WriteProjectConfig( fn.GetFullPath(), wxT( "/pcbnew" ),
                                   ParamCfgList );
}
