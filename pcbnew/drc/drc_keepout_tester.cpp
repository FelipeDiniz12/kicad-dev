/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2020 KiCad Developers, see AUTHORS.txt for contributors.
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


#include <drc/drc_keepout_tester.h>
#include <geometry/shape_segment.h>
#include <class_module.h>
#include <drc/drc.h>
#include <drc/drc_item.h>

DRC_KEEPOUT_TESTER::DRC_KEEPOUT_TESTER( MARKER_HANDLER aMarkerHandler ) :
        DRC_TEST_PROVIDER( std::move( aMarkerHandler ) ),
        m_units( EDA_UNITS::MILLIMETRES ),
        m_board( nullptr ),
        m_zone( nullptr ),
        m_keepoutFlags( 0 )
{
}


bool DRC_KEEPOUT_TESTER::RunDRC( EDA_UNITS aUnits, BOARD& aBoard )
{
    bool success = true;

    m_units = aUnits;
    m_board = &aBoard;

    // Get a list of all zones to inspect, from both board and footprints
    std::list<ZONE_CONTAINER*> areasToInspect = m_board->GetZoneList( true );

    // Test keepout areas for vias, tracks and pads inside keepout areas
    for( ZONE_CONTAINER* area : areasToInspect )
    {
        // JEY TODO: our existing keepout strategy needs a work-over for rules....
        m_keepoutFlags = area->GetKeepouts( F_Cu, &m_sources );

        if( m_keepoutFlags > 0 )
        {
            m_zone = area;
            m_zoneBBox = area->GetBoundingBox();

            success &= checkTracksAndVias();
            success &= checkFootprints();
            success &= checkDrawings();
        }
    }

    return success;
}


bool DRC_KEEPOUT_TESTER::checkTracksAndVias()
{
    constexpr int VIA_MASK = DISALLOW_VIAS | DISALLOW_MICRO_VIAS | DISALLOW_BB_VIAS;
    constexpr int CHECK_VIAS_MASK = VIA_MASK | DISALLOW_HOLES;
    constexpr int CHECK_TRACKS_AND_VIAS_MASK = CHECK_VIAS_MASK | DISALLOW_TRACKS;

    if(( m_keepoutFlags & CHECK_TRACKS_AND_VIAS_MASK ) == 0 )
        return true;

    bool success = true;

    for( TRACK* segm : m_board->Tracks() )
    {
        if( !m_zoneBBox.Intersects( segm->GetBoundingBox() ) )
            continue;

        if( segm->Type() == PCB_TRACE_T && ( m_keepoutFlags & DISALLOW_TRACKS ) != 0 )
        {
            // Ignore if the keepout zone is not on the same layer
            if( !m_zone->IsOnLayer( segm->GetLayer() ) )
                continue;

            SEG trackSeg( segm->GetStart(), segm->GetEnd() );

            if( m_zone->Outline()->Collide( trackSeg, segm->GetWidth() / 2 ) )
            {
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );

                m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ),
                              m_sources.at( DISALLOW_TRACKS ) );

                drcItem->SetErrorMessage( m_msg );
                drcItem->SetItems( segm, m_zone );

                HandleMarker( new MARKER_PCB( drcItem, DRC::GetLocation( segm, m_zone ) ) );
                success = false;
            }
        }
        else if( segm->Type() == PCB_VIA_T && ( m_keepoutFlags & CHECK_VIAS_MASK ) != 0 )
        {
            VIA* via  = static_cast<VIA*>( segm );
            SEG  seg( via->GetPosition(), via->GetPosition() );
            int  test = 0;
            int  clearance = via->GetWidth() / 2;

            if( ( m_keepoutFlags & DISALLOW_VIAS ) > 0 )
            {
                test = DISALLOW_VIAS;
            }
            else if( via->GetViaType() == VIATYPE::MICROVIA
                        && ( m_keepoutFlags & DISALLOW_MICRO_VIAS ) > 0 )
            {
                test = DISALLOW_MICRO_VIAS;
            }
            else if( via->GetViaType() == VIATYPE::BLIND_BURIED
                        && ( m_keepoutFlags & DISALLOW_BB_VIAS ) > 0 )
            {
                test = DISALLOW_BB_VIAS;
            }
            else if( ( m_keepoutFlags & DISALLOW_HOLES ) > 0 )
            {
                test = DISALLOW_HOLES;
                clearance = via->GetDrillValue() / 2;
            }
            else
                continue;

            if( m_zone->Outline()->Collide( seg, clearance ) )
            {
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );
                m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ), m_sources.at( test ) );
                drcItem->SetErrorMessage( m_msg );
                drcItem->SetItems( segm, m_zone );

                HandleMarker( new MARKER_PCB( drcItem, DRC::GetLocation( segm, m_zone ) ) );
                success = false;
            }
        }
    }

    return success;
}


bool DRC_KEEPOUT_TESTER::checkFootprints()
{
    constexpr int CHECK_PADS_MASK = DISALLOW_PADS | DISALLOW_HOLES;
    constexpr int CHECK_FOOTPRINTS_MASK = CHECK_PADS_MASK | DISALLOW_FOOTPRINTS;

    if(( m_keepoutFlags & CHECK_FOOTPRINTS_MASK ) == 0 )
        return true;

    bool success = true;

    for( MODULE* fp : m_board->Modules() )
    {
        if( !m_zoneBBox.Intersects( fp->GetBoundingBox() ) )
            continue;

        if( ( m_keepoutFlags & DISALLOW_FOOTPRINTS ) > 0
                && ( fp->IsFlipped() ? m_zone->CommonLayerExists( LSET::BackMask() )
                                     : m_zone->CommonLayerExists( LSET::FrontMask() ) ) )
        {
            SHAPE_POLY_SET poly;

            if( fp->BuildPolyCourtyard() )
                poly = fp->IsFlipped() ? fp->GetPolyCourtyardBack() : fp->GetPolyCourtyardFront();

            if( poly.OutlineCount() == 0 )
                poly = fp->GetBoundingPoly();

            // Build the common area between footprint and the keepout area:
            poly.BooleanIntersection( *m_zone->Outline(), SHAPE_POLY_SET::PM_FAST );

            // If it's not empty then we have a violation
            if( poly.OutlineCount() )
            {
                const VECTOR2I& pt = poly.CVertex( 0, 0, -1 );
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );

                m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ),
                              m_sources.at( DISALLOW_FOOTPRINTS ) );

                drcItem->SetErrorMessage( m_msg );
                drcItem->SetItems( fp, m_zone );

                HandleMarker( new MARKER_PCB( drcItem, (wxPoint) pt ) );
                success = false;
            }
        }

        if( ( m_keepoutFlags & CHECK_PADS_MASK ) > 0 )
        {
            success &= checkPads( fp );
        }
    }

    return success;
}


bool DRC_KEEPOUT_TESTER::checkPads( MODULE* aModule )
{
    bool success = true;

    for( D_PAD* pad : aModule->Pads() )
    {
        if( !m_zone->CommonLayerExists( pad->GetLayerSet() ) )
            continue;

        // Fast test to detect a pad inside the keepout area bounding box.
        EDA_RECT padBBox( pad->GetPosition(), wxSize() );
        padBBox.Inflate( pad->GetBoundingRadius() );

        if( !m_zoneBBox.Intersects( padBBox ) )
            continue;

        if( ( m_keepoutFlags & DISALLOW_PADS ) > 0 )
        {
            SHAPE_POLY_SET outline = *pad->GetEffectivePolygon();

            // Build the common area between pad and the keepout area:
            outline.BooleanIntersection( *m_zone->Outline(), SHAPE_POLY_SET::PM_FAST );

            // If it's not empty then we have a violation
            if( outline.OutlineCount() )
            {
                const VECTOR2I& pt = outline.CVertex( 0, 0, -1 );
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );

                m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ),
                              m_sources.at( DISALLOW_PADS ) );

                drcItem->SetErrorMessage( m_msg );
                drcItem->SetItems( pad, m_zone );

                HandleMarker( new MARKER_PCB( drcItem, (wxPoint) pt ) );
                success = false;
            }
        }
        else if( ( m_keepoutFlags & DISALLOW_HOLES ) > 0 )
        {
            const SHAPE_SEGMENT* slot = pad->GetEffectiveHoleShape();

            if( m_zone->Outline()->Collide( slot->GetSeg(), slot->GetWidth() / 2 ) )
            {
                std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );

                m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ),
                              m_sources.at( DISALLOW_HOLES ) );

                drcItem->SetErrorMessage( m_msg );
                drcItem->SetItems( pad, m_zone );

                HandleMarker( new MARKER_PCB( drcItem, pad->GetPosition() ) );
                success = false;
            }
        }
    }

    return success;
}


bool DRC_KEEPOUT_TESTER::checkDrawings()
{
    constexpr int CHECK_DRAWINGS_MASK = DISALLOW_TEXTS | DISALLOW_GRAPHICS;
    constexpr KICAD_T graphicTypes[] = { PCB_LINE_T, PCB_DIMENSION_T, PCB_TARGET_T, EOT };

    if(( m_keepoutFlags & CHECK_DRAWINGS_MASK ) == 0 )
        return true;

    bool success = true;

    for( BOARD_ITEM* drawing : m_board->Drawings() )
    {
        if( !m_zoneBBox.Intersects( drawing->GetBoundingBox() ) )
            continue;

        int  sourceId = 0;

        if( drawing->IsType( graphicTypes ) && ( m_keepoutFlags & DISALLOW_GRAPHICS ) > 0 )
            sourceId = DISALLOW_GRAPHICS;
        else if( drawing->Type() == PCB_TEXT_T && ( m_keepoutFlags & DISALLOW_TEXTS ) > 0 )
            sourceId = DISALLOW_TEXTS;
        else
            continue;

        SHAPE_POLY_SET poly;
        drawing->TransformShapeWithClearanceToPolygon( poly, 0 );

        // Build the common area between footprint and the keepout area:
        poly.BooleanIntersection( *m_zone->Outline(), SHAPE_POLY_SET::PM_FAST );

        // If it's not empty then we have a violation
        if( poly.OutlineCount() )
        {
            const VECTOR2I& pt = poly.CVertex( 0, 0, -1 );
            std::shared_ptr<DRC_ITEM> drcItem = DRC_ITEM::Create( DRCE_KEEPOUT );
            m_msg.Printf( drcItem->GetErrorText() + _( " (%s)" ), m_sources.at( sourceId ) );
            drcItem->SetErrorMessage( m_msg );
            drcItem->SetItems( drawing, m_zone );

            HandleMarker( new MARKER_PCB( drcItem, (wxPoint) pt ) );
            success = false;
        }
    }

    return success;
}


