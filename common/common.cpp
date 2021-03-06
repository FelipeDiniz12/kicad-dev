/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2004 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 2008-2011 Wayne Stambaugh <stambaughw@verizon.net>
 * Copyright (C) 1992-2011 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file common.cpp
 */

#include <fctsys.h>
#include <gr_basic.h>
#include <trigo.h>
#include <wxstruct.h>
#include <base_struct.h>
#include <common.h>
#include <macros.h>
#include <build_version.h>
#include <confirm.h>
#include <base_units.h>

#include <wx/process.h>

// Show warning if wxWidgets Gnome or GTK printing was not configured.
// Since wxWidgets 3.0, this is no more needed (build in printing works!)
#if defined( __WXGTK__ )
    #if !wxCHECK_VERSION( 3, 0, 0 )
    #   if !wxUSE_LIBGNOMEPRINT && !wxUSE_GTKPRINT && !SWIG
    #       warning "You must use '--with-gnomeprint' or '--with-gtkprint' in your wx library configuration for full print capabilities."
    #   endif
    #endif
#endif

/**
 * Global variables definitions.
 *
 * TODO: All of these variables should be moved into the class were they
 *       are defined and used.  Most of them probably belong in the
 *       application class.
 */

bool           g_ShowPageLimits = true;
wxString       g_UserLibDirBuffer;

EDA_UNITS_T    g_UserUnit;
EDA_COLOR_T    g_GhostColor;


/**
 * The predefined colors used in KiCad.
 * Please: if you change a value, remember these values are carefully chosen
 * to have good results in Pcbnew, that uses the ORed value of basic colors
 * when displaying superimposed objects
 * This list must have exactly NBCOLORS items
 */
const StructColors g_ColorRefs[NBCOLORS] =
{
    { 0,    0,   0,   BLACK,         wxT( "Black" ),     DARKDARKGRAY      },
    { 72,   72,  72,  DARKDARKGRAY,  wxT( "Gray 1" ),    DARKGRAY          },
    { 132,  132, 132, DARKGRAY,      wxT( "Gray 2" ),    LIGHTGRAY         },
    { 194,  194, 194, LIGHTGRAY,     wxT( "Gray 3" ),    WHITE             },
    { 255,  255, 255, WHITE,         wxT( "White" ),     WHITE             },
    { 194,  255, 255, LIGHTYELLOW,   wxT( "L.Yellow" ),  WHITE             },
    { 72,   0,   0,   DARKBLUE,      wxT( "Blue 1" ),    BLUE              },
    { 0,    72,  0,   DARKGREEN,     wxT( "Green 1" ),   GREEN             },
    { 72,   72,  0,   DARKCYAN,      wxT( "Cyan 1" ),    CYAN              },
    { 0,    0,   72,  DARKRED,       wxT( "Red 1" ),     RED               },
    { 72,   0,   72,  DARKMAGENTA,   wxT( "Magenta 1" ), MAGENTA           },
    { 0,    72,  72,  DARKBROWN,     wxT( "Brown 1" ),   BROWN             },
    { 132,  0,   0,   BLUE,          wxT( "Blue 2" ),    LIGHTBLUE         },
    { 0,    132, 0,   GREEN,         wxT( "Green 2" ),   LIGHTGREEN        },
    { 132,  132, 0,   CYAN,          wxT( "Cyan 2" ),    LIGHTCYAN         },
    { 0,    0,   132, RED,           wxT( "Red 2" ),     LIGHTRED          },
    { 132,  0,   132, MAGENTA,       wxT( "Magenta 2" ), LIGHTMAGENTA      },
    { 0,    132, 132, BROWN,         wxT( "Brown 2" ),   YELLOW            },
    { 194,  0,   0,   LIGHTBLUE,     wxT( "Blue 3" ),    PUREBLUE,         },
    { 0,    194, 0,   LIGHTGREEN,    wxT( "Green 3" ),   PUREGREEN         },
    { 194,  194, 0,   LIGHTCYAN,     wxT( "Cyan 3" ),    PURECYAN          },
    { 0,    0,   194, LIGHTRED,      wxT( "Red 3" ),     PURERED           },
    { 194,  0,   194, LIGHTMAGENTA,  wxT( "Magenta 3" ), PUREMAGENTA       },
    { 0,    194, 194, YELLOW,        wxT( "Yellow 3" ),  PUREYELLOW        },
    { 255,  0,   0,   PUREBLUE,      wxT( "Blue 4" ),    WHITE             },
    { 0,    255, 0,   PUREGREEN,     wxT( "Green 4" ),   WHITE             },
    { 255,  255, 0,   PURECYAN,      wxT( "Cyan 4" ),    WHITE             },
    { 0,    0,   255, PURERED,       wxT( "Red 4" ),     WHITE             },
    { 255,  0,   255, PUREMAGENTA,   wxT( "Magenta 4" ), WHITE             },
    { 0,    255, 255, PUREYELLOW,    wxT( "Yellow 4" ),  WHITE             },
};


/**
 * Function to use local notation or C standard notation for floating point numbers
 * some countries use 1,5 and others (and C) 1.5
 * so we switch from local to C and C to local when reading or writing files
 * And other problem is a bug when cross compiling under linux:
 * a printf print 1,5 and the read functions expects 1.5
 * (depending on version print = 1.5 and read = 1,5
 * Very annoying and we detect this and use a stupid but necessary workaround
*/
bool g_DisableFloatingPointLocalNotation = false;


int LOCALE_IO::C_count;


void SetLocaleTo_C_standard()
{
    setlocale( LC_NUMERIC, "C" );    // Switch the locale to standard C
}

void SetLocaleTo_Default()
{
    if( !g_DisableFloatingPointLocalNotation )
        setlocale( LC_NUMERIC, "" );      // revert to the current locale
}


wxSize GetTextSize( const wxString& aSingleLine, wxWindow* aWindow )
{
    wxCoord width;
    wxCoord height;

    {
        wxClientDC dc( aWindow );
        dc.SetFont( aWindow->GetFont() );
        dc.GetTextExtent( aSingleLine, &width, &height );
    }

    return wxSize( width, height );
}


bool EnsureTextCtrlWidth( wxTextCtrl* aCtrl, const wxString* aString )
{
    wxWindow* window = aCtrl->GetParent();

    if( !window )
        window = aCtrl;

    wxString ctrlText;

    if( !aString )
    {
        ctrlText = aCtrl->GetValue();
        aString  = &ctrlText;
    }

    wxSize  textz = GetTextSize( *aString, window );
    wxSize  ctrlz = aCtrl->GetSize();

    if( ctrlz.GetWidth() < textz.GetWidth() + 10 )
    {
        ctrlz.SetWidth( textz.GetWidth() + 10 );
        aCtrl->SetSizeHints( ctrlz );
        return true;
    }

    return false;
}


wxString ReturnUnitSymbol( EDA_UNITS_T aUnit, const wxString& formatString )
{
    wxString tmp;
    wxString label;

    switch( aUnit )
    {
    case INCHES:
        tmp = _( "\"" );
        break;

    case MILLIMETRES:
        tmp = _( "mm" );
        break;

    case UNSCALED_UNITS:
        break;
    }

    if( formatString.IsEmpty() )
        return tmp;

    label.Printf( formatString, GetChars( tmp ) );

    return label;
}


wxString GetUnitsLabel( EDA_UNITS_T aUnit )
{
    wxString label;

    switch( aUnit )
    {
    case INCHES:
        label = _( "inches" );
        break;

    case MILLIMETRES:
        label = _( "millimeters" );
        break;

    case UNSCALED_UNITS:
        label = _( "units" );
        break;
    }

    return label;
}


wxString GetAbbreviatedUnitsLabel( EDA_UNITS_T aUnit )
{
    wxString label;

    switch( aUnit )
    {
    case INCHES:
        label = _( "in" );
        break;

    case MILLIMETRES:
        label = _( "mm" );
        break;

    case UNSCALED_UNITS:
        break;
    }

    return label;
}


void AddUnitSymbol( wxStaticText& Stext, EDA_UNITS_T aUnit )
{
    wxString msg = Stext.GetLabel();

    msg += ReturnUnitSymbol( aUnit );

    Stext.SetLabel( msg );
}


wxArrayString* wxStringSplit( wxString aString, wxChar aSplitter )
{
    wxArrayString* list = new wxArrayString();

    while( 1 )
    {
        int index = aString.Find( aSplitter );

        if( index == wxNOT_FOUND )
            break;

        wxString tmp;
        tmp = aString.Mid( 0, index );
        aString = aString.Mid( index + 1, aString.size() - index );
        list->Add( tmp );
    }

    if( !aString.IsEmpty() )
    {
        list->Add( aString );
    }

    return list;
}


int ProcessExecute( const wxString& aCommandLine, int aFlags, wxProcess *callback )
{
    return wxExecute( aCommandLine, aFlags, callback );
}


time_t GetNewTimeStamp()
{
    static time_t oldTimeStamp;
    time_t newTimeStamp;

    newTimeStamp = time( NULL );

    if( newTimeStamp <= oldTimeStamp )
        newTimeStamp = oldTimeStamp + 1;

    oldTimeStamp = newTimeStamp;

    return newTimeStamp;
}


double RoundTo0( double x, double precision )
{
    assert( precision != 0 );

    long long ix = KiROUND( x * precision );

    if ( x < 0.0 )
        NEGATE( ix );

    int remainder = ix % 10;   // remainder is in precision mm

    if ( remainder <= 2 )
        ix -= remainder;       // truncate to the near number
    else if (remainder >= 8 )
        ix += 10 - remainder;  // round to near number

    if ( x < 0 )
        NEGATE( ix );

    return (double) ix / precision;
}

wxString FormatDateLong( const wxDateTime &aDate )
{
    /* GetInfo was introduced only on wx 2.9; for portability reason an
     * hardcoded format is used on wx 2.8 */
#if wxCHECK_VERSION( 2, 9, 0 )
    return aDate.Format( wxLocale::GetInfo( wxLOCALE_LONG_DATE_FMT ) );
#else
    return aDate.Format( wxT("%d %b %Y") );
#endif
}

