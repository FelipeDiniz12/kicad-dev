/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 1992-2013 KiCad Developers, see AUTHORS.txt for contributors.
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
 * @file gerbview/initpcb.cpp
 */

#include <fctsys.h>
#include <common.h>
#include <class_drawpanel.h>
#include <confirm.h>

#include <gerbview.h>
#include <class_gerber_draw_item.h>
#include <class_GERBER.h>
#include <class_gerbview_layer_widget.h>
#include <class_gbr_layout.h>

bool GERBVIEW_FRAME::Clear_Pcb( bool query )
{
    LAYER_NUM layer;

    if( GetGerberLayout() == NULL )
        return false;

    if( query && GetScreen()->IsModify() )
    {
        if( !IsOK( this, _( "Current data will be lost?" ) ) )
            return false;
    }

    GetGerberLayout()->m_Drawings.DeleteAll();

    for( layer = FIRST_LAYER; layer < NB_GERBER_LAYERS; ++layer )
    {
        if( g_GERBER_List[layer] )
        {
            g_GERBER_List[layer]->InitToolTable();
            g_GERBER_List[layer]->ResetDefaultValues();
        }
    }

    GetGerberLayout()->SetBoundingBox( EDA_RECT() );

    
    wxSize pageSize;

    pageSize.x = Units()->MilsToIu ( GetPageSettings().GetWidthMils() );
    pageSize.y = Units()->MilsToIu ( GetPageSettings().GetHeightMils() );

    SetScreen( new GBR_SCREEN( pageSize ) );

    setActiveLayer( FIRST_LAYER );
    m_LayersManager->UpdateLayerIcons();
    syncLayerBox();
    return true;
}


void GERBVIEW_FRAME::Erase_Current_Layer( bool query )
{
    LAYER_NUM layer = getActiveLayer();
    wxString msg;

    msg.Printf( _( "Clear layer %d?" ), layer + 1 );

    if( query && !IsOK( this, msg ) )
        return;

    SetCurItem( NULL );

    GERBER_DRAW_ITEM* item = GetGerberLayout()->m_Drawings;
    GERBER_DRAW_ITEM * next;

    for( ; item; item = next )
    {
        next = item->Next();

        if( item->GetLayer() != layer )
            continue;

        item->DeleteStructure();
    }

    if( g_GERBER_List[layer] )
    {
        g_GERBER_List[layer]->InitToolTable();
        g_GERBER_List[layer]->ResetDefaultValues();
    }

    GetScreen()->SetModify();
    m_canvas->Refresh();
    m_LayersManager->UpdateLayerIcons();
    syncLayerBox();
}
