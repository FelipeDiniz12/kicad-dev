/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 1992-2015 KiCad Developers, see AUTHORS.txt for contributors.
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
 * @file class_pcb_text.cpp
 * @brief Class TEXTE_PCB texts on copper or technical layers implementation
 */

#include <fctsys.h>
#include <gr_basic.h>
#include <base_struct.h>
#include <gr_text.h>
#include <pcb_edit_frame.h>
#include <base_units.h>
#include <bitmaps.h>
#include <settings/color_settings.h>
#include <settings/settings_manager.h>

#include <class_board.h>
#include <class_pcb_text.h>
#include <pcb_painter.h>

using KIGFX::PCB_RENDER_SETTINGS;


TEXTE_PCB::TEXTE_PCB( BOARD_ITEM* parent ) :
    BOARD_ITEM( parent, PCB_TEXT_T ),
    EDA_TEXT()
{
    SetMultilineAllowed( true );
}


TEXTE_PCB::~TEXTE_PCB()
{
}


wxString TEXTE_PCB::GetShownText( int aDepth ) const
{
    BOARD* board = static_cast<BOARD*>( GetParent() );
    wxASSERT( board );

    std::function<bool( wxString* )> pcbTextResolver =
            [&]( wxString* token ) -> bool
            {
                if( token->IsSameAs( wxT( "LAYER" ) ) )
                {
                    *token = GetLayerName();
                    return true;
                }

                if( token->Contains( ':' ) )
                {
                    wxString      remainder;
                    wxString      ref = token->BeforeFirst( ':', &remainder );
                    BOARD_ITEM*   refItem = board->GetItem( KIID( ref ) );

                    if( refItem && refItem->Type() == PCB_MODULE_T )
                    {
                        MODULE* refModule = static_cast<MODULE*>( refItem );

                        if( refModule->ResolveTextVar( &remainder, aDepth + 1 ) )
                        {
                            *token = remainder;
                            return true;
                        }
                    }
                }
                return false;
            };

    bool     processTextVars = false;
    wxString text = EDA_TEXT::GetShownText( &processTextVars );

    if( processTextVars && aDepth < 10 )
        text = ExpandTextVars( text, &pcbTextResolver, board->GetProject() );

    return text;
}


void TEXTE_PCB::SetTextAngle( double aAngle )
{
    EDA_TEXT::SetTextAngle( NormalizeAngle360Min( aAngle ) );
}


void TEXTE_PCB::GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList )
{
    wxString    msg;

    wxCHECK_RET( m_Parent != NULL, wxT( "TEXTE_PCB::GetMsgPanelInfo() m_Parent is NULL." ) );

    if( m_Parent->Type() == PCB_DIMENSION_T )
        aList.emplace_back( _( "Dimension" ), GetShownText(), DARKGREEN );
    else
        aList.emplace_back( _( "PCB Text" ), GetShownText(), DARKGREEN );

    aList.emplace_back( _( "Layer" ), GetLayerName(), BLUE );

    if( !IsMirrored() )
        aList.emplace_back( _( "Mirror" ), _( "No" ), DARKGREEN );
    else
        aList.emplace_back( _( "Mirror" ), _( "Yes" ), DARKGREEN );

    msg.Printf( wxT( "%.1f" ), GetTextAngle() / 10.0 );
    aList.emplace_back( _( "Angle" ), msg, DARKGREEN );

    msg = MessageTextFromValue( aFrame->GetUserUnits(), GetTextThickness() );
    aList.emplace_back( _( "Thickness" ), msg, MAGENTA );

    msg = MessageTextFromValue( aFrame->GetUserUnits(), GetTextWidth() );
    aList.emplace_back( _( "Width" ), msg, RED );

    msg = MessageTextFromValue( aFrame->GetUserUnits(), GetTextHeight() );
    aList.emplace_back( _( "Height" ), msg, RED );
}


const EDA_RECT TEXTE_PCB::GetBoundingBox() const
{
    EDA_RECT rect = GetTextBox();

    if( GetTextAngle() )
        rect = rect.GetBoundingBoxRotated( GetTextPos(), GetTextAngle() );

    return rect;
}


void TEXTE_PCB::Rotate( const wxPoint& aRotCentre, double aAngle )
{
    wxPoint pt = GetTextPos();
    RotatePoint( &pt, aRotCentre, aAngle );
    SetTextPos( pt );

    SetTextAngle( GetTextAngle() + aAngle );
}


void TEXTE_PCB::Flip( const wxPoint& aCentre, bool aFlipLeftRight )
{
    double angle = GetTextAngle();
    bool   vertical = KiROUND( angle ) % 1800 == 900;

    if( KiROUND( angle ) != 0 )
    {
        Rotate( aCentre, -angle );

        if( vertical )
            aFlipLeftRight = !aFlipLeftRight;
    }

    // Flip the bounding box
    EDA_RECT box = GetTextBox();
    int      left = box.GetLeft();
    int      right = box.GetRight();
    int      top = box.GetTop();
    int      bottom = box.GetBottom();

    if( aFlipLeftRight )
    {
        MIRROR( left, aCentre.x );
        MIRROR( right, aCentre.x );
        std::swap( left, right );
    }
    else
    {
        MIRROR( top, aCentre.y );
        MIRROR( bottom, aCentre.y );
        std::swap( top, bottom );
    }

    // Now put the text back in it (these look backwards but remember that out text will
    // be mirrored when all is said and done)
    switch( GetHorizJustify() )
    {
    case GR_TEXT_HJUSTIFY_LEFT:   SetTextX( right );                break;
    case GR_TEXT_HJUSTIFY_CENTER: SetTextX( ( left + right ) / 2 ); break;
    case GR_TEXT_HJUSTIFY_RIGHT:  SetTextX( left );                 break;
    }

    switch( GetVertJustify() )
    {
    case GR_TEXT_VJUSTIFY_TOP:    SetTextY( bottom );               break;
    case GR_TEXT_VJUSTIFY_CENTER: SetTextY( ( top + bottom ) / 2 ); break;
    case GR_TEXT_VJUSTIFY_BOTTOM: SetTextY( top );                  break;
    }

    // And restore orientation
    if( KiROUND( angle ) != 0 )
        Rotate( aCentre, angle );

    SetLayer( FlipLayer( GetLayer(), GetBoard()->GetCopperLayerCount() ) );
    SetMirrored( !IsMirrored() );
}


wxString TEXTE_PCB::GetSelectMenuText( EDA_UNITS aUnits ) const
{
    return wxString::Format( _( "Pcb Text \"%s\" on %s"), ShortenedShownText(), GetLayerName() );
}


BITMAP_DEF TEXTE_PCB::GetMenuImage() const
{
    return text_xpm;
}


EDA_ITEM* TEXTE_PCB::Clone() const
{
    return new TEXTE_PCB( *this );
}


void TEXTE_PCB::SwapData( BOARD_ITEM* aImage )
{
    assert( aImage->Type() == PCB_TEXT_T );

    std::swap( *((TEXTE_PCB*) this), *((TEXTE_PCB*) aImage) );
}


std::shared_ptr<SHAPE> TEXTE_PCB::GetEffectiveShape( PCB_LAYER_ID aLayer ) const
{
    return GetEffectiveTextShape();
}


static struct TEXTE_PCB_DESC
{
    TEXTE_PCB_DESC()
    {
        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( TEXTE_PCB );
        propMgr.AddTypeCast( new TYPE_CAST<TEXTE_PCB, BOARD_ITEM> );
        propMgr.AddTypeCast( new TYPE_CAST<TEXTE_PCB, EDA_TEXT> );
        propMgr.InheritsAfter( TYPE_HASH( TEXTE_PCB ), TYPE_HASH( BOARD_ITEM ) );
        propMgr.InheritsAfter( TYPE_HASH( TEXTE_PCB ), TYPE_HASH( EDA_TEXT ) );
    }
} _TEXTE_PCB_DESC;
