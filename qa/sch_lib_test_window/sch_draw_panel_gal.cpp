/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014-2017 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
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


#include <view/wx_view_controls.h>
#include <worksheet_viewitem.h>

#include <gal/graphics_abstraction_layer.h>

#include "sch_draw_panel_gal.h"
#include "sch_view.h"
#include "sch_painter.h"

#include <functional>

#include <sch_sheet.h>

using namespace std::placeholders;


SCH_DRAW_PANEL_GAL::SCH_DRAW_PANEL_GAL( wxWindow* aParentWindow, wxWindowID aWindowId,
                                        const wxPoint& aPosition, const wxSize& aSize,
                                        KIGFX::GAL_DISPLAY_OPTIONS& aOptions, GAL_TYPE aGalType ) :
EDA_DRAW_PANEL_GAL( aParentWindow, aWindowId, aPosition, aSize, aOptions, aGalType )
{
    m_view = new KIGFX::SCH_VIEW( true );
    m_view->SetGAL( m_gal );


    m_painter.reset( new KIGFX::SCH_PAINTER( m_gal ) );

    m_view->SetPainter( m_painter.get() );
    m_view->SetScaleLimits( 1000000.0, 0.001 );
    m_view->SetMirror( false, false );

    setDefaultLayerOrder();
    setDefaultLayerDeps();

    view()->UpdateAllLayersOrder();
    // View controls is the first in the event handler chain, so the Tool Framework operates
    // on updated viewport data.
    m_viewControls = new KIGFX::WX_VIEW_CONTROLS( m_view, this );
}


SCH_DRAW_PANEL_GAL::~SCH_DRAW_PANEL_GAL()
{
}


void SCH_DRAW_PANEL_GAL::DisplayComponent( const LIB_PART* aComponent )
{
    view()->Clear();
    view()->DisplayComponent( const_cast<LIB_PART*>(aComponent) );

}

void SCH_DRAW_PANEL_GAL::DisplaySheet( const SCH_SHEET* aSheet )
{
    view()->Clear();
    view()->DisplaySheet( const_cast<SCH_SHEET*>(aSheet) );
}


void SCH_DRAW_PANEL_GAL::OnShow()
{

    //m_view->RecacheAllItems();
}


void SCH_DRAW_PANEL_GAL::setDefaultLayerOrder()
{
/*    for( LAYER_NUM i = 0; (unsigned) i < sizeof( GAL_LAYER_ORDER ) / sizeof( LAYER_NUM ); ++i )
    {
        LAYER_NUM layer = GAL_LAYER_ORDER[i];
        wxASSERT( layer < KIGFX::VIEW::VIEW_MAX_LAYERS );

        m_view->SetLayerOrder( layer, i );
    }*/
}


bool SCH_DRAW_PANEL_GAL::SwitchBackend( GAL_TYPE aGalType )
{
    bool rv = EDA_DRAW_PANEL_GAL::SwitchBackend( aGalType );
    setDefaultLayerDeps();
    return rv;
}



void SCH_DRAW_PANEL_GAL::setDefaultLayerDeps()
{
    // caching makes no sense for Cairo and other software renderers
    auto target = m_backend == GAL_TYPE_OPENGL ? KIGFX::TARGET_CACHED : KIGFX::TARGET_NONCACHED;

    for( int i = 0; i < KIGFX::VIEW::VIEW_MAX_LAYERS; i++ )
        m_view->SetLayerTarget( i, target );

    m_view->SetLayerTarget( LAYER_GP_OVERLAY , KIGFX::TARGET_OVERLAY );
    m_view->SetLayerDisplayOnly( LAYER_GP_OVERLAY ) ;

    m_view->SetLayerDisplayOnly( LAYER_WORKSHEET ) ;
    m_view->SetLayerDisplayOnly( LAYER_DRC );
}


KIGFX::SCH_VIEW* SCH_DRAW_PANEL_GAL::view() const
{
    return static_cast<KIGFX::SCH_VIEW*>( m_view );
}
