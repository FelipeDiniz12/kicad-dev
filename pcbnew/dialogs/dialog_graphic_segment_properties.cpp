/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015-2017 CERN
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

#include <dialogs/dialog_graphic_segment_properties.h>
#include <class_pcb_layer_box_selector.h>
#include <tools/selection_tool.h>
#include <class_drawsegment.h>
#include <wxPcbStruct.h>
#include <confirm.h>

#include <widgets/widget_net_selector.h>
#include <board_commit.h>

template<class Container>
static boost::optional<int> uniqueFieldValue( const Container& cont, std::function< int(const BOARD_ITEM *)> getField )
{
    int prev = 0;
    bool firstItem = true;
    for ( auto item : cont )
    {
        int val = getField( static_cast<BOARD_ITEM *> ( item ) );
        if( !firstItem && val != prev )
            return boost::optional<int>();
        prev = val;
        firstItem = false;
    }

    return prev;
}

//template<class T>
//std::function<T(const BOARD_ITEM*)> t = [] ( const BOARD_ITEM* item ) -> int { return 0; };


DIALOG_GRAPHIC_SEGMENT_PROPERTIES::DIALOG_GRAPHIC_SEGMENT_PROPERTIES( PCB_BASE_FRAME* aParent, const SELECTION& aItems ) :
    DIALOG_GRAPHIC_SEGMENT_PROPERTIES_BASE( aParent ), m_items( aItems ),
    m_startX( aParent, m_startXCtrl, m_startXUnit ),
    m_startY( aParent, m_startYCtrl, m_startYUnit ),
    m_endX( aParent, m_endXCtrl, m_endXUnit ),
    m_endY( aParent, m_endYCtrl, m_endYUnit ),
    m_width( aParent, m_widthCtrl, m_widthUnit )
{
    assert( !m_items.Empty() );

    printf("Items: %d\n", m_items.Size() );

    setCommonVal( uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetStart().x;
    } ), m_startXCtrl, m_startX );

    setCommonVal( uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetStart().y;
    } ), m_startYCtrl, m_startY );

    setCommonVal( uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetEnd().x;
    } ), m_endXCtrl, m_endX );

    setCommonVal( uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetEnd().y;
    } ), m_endYCtrl, m_endY );

    setCommonVal( uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetWidth();
    } ), m_widthCtrl, m_width );

    auto layer = uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetLayer();
    } );

    m_layerCtrl->SetLayersHotkeys( false );
    m_layerCtrl->SetLayerSet( LSET::AllNonCuMask() );
    m_layerCtrl->SetBoardFrame( aParent );
    m_layerCtrl->Resync();

    if( layer )
        m_layerCtrl->SetLayerSelection( *layer );

    auto fixLength = uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetUserFlags() & DSF_CONSTRAIN_LENGTH;
    } );

    auto fixDirection = uniqueFieldValue( m_items, [] ( const BOARD_ITEM* item ) -> int {
        return static_cast<const DRAWSEGMENT*> (item)->GetUserFlags() & DSF_CONSTRAIN_DIRECTION;
    } );


    if( fixLength )
        m_fixLength->Set3StateValue( *fixLength ? wxCHK_CHECKED : wxCHK_UNCHECKED );
    else
        m_fixLength->Set3StateValue( wxCHK_UNDETERMINED );

    if( fixDirection )
        m_fixDirection->Set3StateValue( *fixDirection ? wxCHK_CHECKED : wxCHK_UNCHECKED );
    else
        m_fixDirection->Set3StateValue( wxCHK_UNDETERMINED );


    m_stdButtonsOK->SetDefault();

    // Pressing ENTER when any of the text input fields is active applies changes
    Connect( wxEVT_TEXT_ENTER, wxCommandEventHandler( DIALOG_GRAPHIC_SEGMENT_PROPERTIES::onOkClick ), NULL, this );
//    #endif
}


bool DIALOG_GRAPHIC_SEGMENT_PROPERTIES::Apply( COMMIT& aCommit )
{
    if( !check() )
        return false;

    for( auto item : m_items )
    {
        auto ds = static_cast<DRAWSEGMENT*> ( item );
        aCommit.Modify( ds );

        wxPoint start = ds->GetStart();

        if( m_startX.Valid() )
            start.x = m_startX.GetValue();
        if( m_startY.Valid() )
            start.y = m_startY.GetValue();

        ds->SetStart( start );

        wxPoint end = ds->GetEnd();

        if( m_endX.Valid() )
            end.x = m_endX.GetValue();
        if( m_endY.Valid() )
            end.y = m_endY.GetValue();

        ds->SetEnd( end );

        if( m_width.Valid() )
            ds->SetWidth( m_width.GetValue() );

        LAYER_NUM layer = m_layerCtrl->GetLayerSelection();

        if( layer != UNDEFINED_LAYER )
            ds->SetLayer( (PCB_LAYER_ID) layer );

        if( m_fixDirection->Get3StateValue() != wxCHK_UNDETERMINED )
            ds->SetUserFlags( DSF_CONSTRAIN_DIRECTION, m_fixDirection->Get3StateValue() == wxCHK_CHECKED );
        if( m_fixLength->Get3StateValue() != wxCHK_UNDETERMINED )
            ds->SetUserFlags( DSF_CONSTRAIN_LENGTH, m_fixLength->Get3StateValue() == wxCHK_CHECKED );

    }

    return true;
}


void DIALOG_GRAPHIC_SEGMENT_PROPERTIES::onClose( wxCloseEvent& aEvent )
{
    EndModal( 0 );
}


void DIALOG_GRAPHIC_SEGMENT_PROPERTIES::onCancelClick( wxCommandEvent& aEvent )
{
    EndModal( 0 );
}


void DIALOG_GRAPHIC_SEGMENT_PROPERTIES::onOkClick( wxCommandEvent& aEvent )
{
    if( check() )
        EndModal( 1 );
}


bool DIALOG_GRAPHIC_SEGMENT_PROPERTIES::check() const
{
    return true;
}
