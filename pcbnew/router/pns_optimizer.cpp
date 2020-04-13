/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013-2014 CERN
 * Copyright (C) 2016 KiCad Developers, see AUTHORS.txt for contributors.
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <geometry/shape_line_chain.h>
#include <geometry/shape_rect.h>
#include <geometry/shape_simple.h>
#include <geometry/shape_file_io.h>

#include <cmath>

#include "pns_arc.h"
#include "pns_line.h"
#include "pns_diff_pair.h"
#include "pns_node.h"
#include "pns_solid.h"
#include "pns_optimizer.h"
#include "pns_topology.h"

#include "pns_utils.h"
#include "pns_router.h"
#include "pns_debug_decorator.h"


namespace PNS {


static DEBUG_DECORATOR *g_dbg;
/**
 *  Cost Estimator Methods
 */
int COST_ESTIMATOR::CornerCost( const SEG& aA, const SEG& aB )
{
    DIRECTION_45 dir_a( aA ), dir_b( aB );

    switch( dir_a.Angle( dir_b ) )
    {
    case DIRECTION_45::ANG_OBTUSE:
        return 10;

    case DIRECTION_45::ANG_STRAIGHT:
        return 5;

    case DIRECTION_45::ANG_ACUTE:
        return 50;

    case DIRECTION_45::ANG_RIGHT:
        return 30;

    case DIRECTION_45::ANG_HALF_FULL:
        return 60;

    default:
        return 100;
    }
}


int COST_ESTIMATOR::CornerCost( const SHAPE_LINE_CHAIN& aLine )
{
    int total = 0;

    for( int i = 0; i < aLine.SegmentCount() - 1; ++i )
        total += CornerCost( aLine.CSegment( i ), aLine.CSegment( i + 1 ) );

    return total;
}


int COST_ESTIMATOR::CornerCost( const LINE& aLine )
{
    return CornerCost( aLine.CLine() );
}


void COST_ESTIMATOR::Add( LINE& aLine )
{
    m_lengthCost += aLine.CLine().Length();
    m_cornerCost += CornerCost( aLine );
}


void COST_ESTIMATOR::Remove( LINE& aLine )
{
    m_lengthCost -= aLine.CLine().Length();
    m_cornerCost -= CornerCost( aLine );
}


void COST_ESTIMATOR::Replace( LINE& aOldLine, LINE& aNewLine )
{
    m_lengthCost -= aOldLine.CLine().Length();
    m_cornerCost -= CornerCost( aOldLine );
    m_lengthCost += aNewLine.CLine().Length();
    m_cornerCost += CornerCost( aNewLine );
}


bool COST_ESTIMATOR::IsBetter( COST_ESTIMATOR& aOther,
        double aLengthTolerance,
        double aCornerTolerance ) const
{
    if( aOther.m_cornerCost < m_cornerCost && aOther.m_lengthCost < m_lengthCost )
        return true;

    else if( aOther.m_cornerCost < m_cornerCost * aCornerTolerance &&
             aOther.m_lengthCost < m_lengthCost * aLengthTolerance )
        return true;

    return false;
}


/**
 *  Optimizer
 **/
OPTIMIZER::OPTIMIZER( NODE* aWorld ) :
    m_world( aWorld ),
    m_collisionKindMask( ITEM::ANY_T ),
    m_effortLevel( MERGE_SEGMENTS ),
    m_keepPostures( false ),
    m_restrictAreaActive( false )
{
}


OPTIMIZER::~OPTIMIZER()
{
}


struct OPTIMIZER::CACHE_VISITOR
{
    CACHE_VISITOR( const ITEM* aOurItem, NODE* aNode, int aMask ) :
        m_ourItem( aOurItem ),
        m_collidingItem( NULL ),
        m_node( aNode ),
        m_mask( aMask )
    {}

    bool operator()( ITEM* aOtherItem )
    {
        if( !( m_mask & aOtherItem->Kind() ) )
            return true;

        int clearance = m_node->GetClearance( aOtherItem, m_ourItem );

        if( !aOtherItem->Collide( m_ourItem, clearance, false, nullptr, m_node ) )
            return true;

        m_collidingItem = aOtherItem;
        return false;
    }

    const ITEM* m_ourItem;
    ITEM* m_collidingItem;
    NODE* m_node;
    int m_mask;
};


void OPTIMIZER::cacheAdd( ITEM* aItem, bool aIsStatic = false )
{
    if( m_cacheTags.find( aItem ) != m_cacheTags.end() )
        return;

    m_cache.Add( aItem );
    m_cacheTags[aItem].m_hits = 1;
    m_cacheTags[aItem].m_isStatic = aIsStatic;
}


void OPTIMIZER::removeCachedSegments( LINE* aLine, int aStartVertex, int aEndVertex )
{
    if( !aLine->IsLinked() ) return;

    auto links = aLine->Links();

    if( aEndVertex < 0 )
        aEndVertex += aLine->PointCount();

    for( int i = aStartVertex; i < aEndVertex - 1; i++ )
    {
        LINKED_ITEM* s = links[i];
        m_cacheTags.erase( s );
        m_cache.Remove( s );
    }
}


void OPTIMIZER::CacheRemove( ITEM* aItem )
{
    if( aItem->Kind() == ITEM::LINE_T )
        removeCachedSegments( static_cast<LINE*>( aItem ) );
}


void OPTIMIZER::CacheStaticItem( ITEM* aItem )
{
    cacheAdd( aItem, true );
}


void OPTIMIZER::ClearCache( bool aStaticOnly  )
{
    if( !aStaticOnly )
    {
        m_cacheTags.clear();
        m_cache.Clear();
        return;
    }

    for( CachedItemTags::iterator i = m_cacheTags.begin(); i!= m_cacheTags.end(); ++i )
    {
        if( i->second.m_isStatic )
        {
            m_cache.Remove( i->first );
            m_cacheTags.erase( i->first );
        }
    }
}


bool ANGLE_CONSTRAINT_45::Check ( int aVertex1, int aVertex2, LINE* aOriginLine, const SHAPE_LINE_CHAIN& aCurrentPath, const SHAPE_LINE_CHAIN& aReplacement )
{
    auto dir_orig0 = DIRECTION_45( aOriginLine->CSegment( aVertex1 ) );
    auto dir_orig1 = DIRECTION_45( aOriginLine->CSegment( aVertex2 - 1) );

    if( aVertex1 == 0 )
    {
        if( ( dir_orig0.Mask() & m_entryDirectionMask ) == 0 )
            return false; // disallowed entry angle
    }

    if( aVertex2 == aOriginLine->SegmentCount() - 1 )
        {
        if( ( dir_orig1.Mask() & m_exitDirectionMask ) == 0 )
            return false; // disallowed exit ngle
            }



    /*auto ang_rep0 = DIRECTION_45( aReplacement.CSegment(0) ).Angle( dir_orig0 );
    auto ang_rep1 = DIRECTION_45( aReplacement.CSegment(-1) ).Angle( dir_orig1 );*/

    return true;
}

bool AREA_CONSTRAINT::Check ( int aVertex1, int aVertex2, LINE* aOriginLine, const SHAPE_LINE_CHAIN& aCurrentPath, const SHAPE_LINE_CHAIN& aReplacement )
{
    auto p1 = aOriginLine->CPoint( aVertex1 );
    auto p2 = aOriginLine->CPoint( aVertex2 );

    auto p1_in = m_allowedArea.Contains( p1 );
    auto p2_in = m_allowedArea.Contains( p2 );

    return p1_in || p2_in;
}


bool PRESERVE_VERTEX_CONSTRAINT::Check ( int aVertex1, int aVertex2, LINE* aOriginLine, const SHAPE_LINE_CHAIN& aCurrentPath, const SHAPE_LINE_CHAIN& aReplacement )
{
    bool cv = false;

    for( int i = aVertex1; i < aVertex2; i++ )
    {
        int dist = aCurrentPath.CSegment(i).Distance( m_v );
        if ( dist <= 1 )
        {
            cv = true;
            break;
        }
    }

    if(!cv)
    {
        return true;
    }

    for( int i = 0; i < aReplacement.SegmentCount(); i++ )
    {
        int dist = aReplacement.CSegment(i).Distance( m_v );

        if ( dist <= 1 )
        {
            return true;
        }
    }

    return false;
}




bool KEEP_TOPOLOGY_CONSTRAINT::Check ( int aVertex1, int aVertex2, LINE* aOriginLine, const SHAPE_LINE_CHAIN& aCurrentPath, const SHAPE_LINE_CHAIN& aReplacement )
{
    SHAPE_LINE_CHAIN encPoly = aOriginLine->CLine().Slice( aVertex1, aVertex2 );

    // fixme: this is a remarkably shitty implementation...
    encPoly.Append( aReplacement.Reverse() );
    encPoly.SetClosed( true );

    auto bb = encPoly.BBox();
    std::vector<JOINT*> joints;

    int cnt = m_world->QueryJoints( bb, joints, aOriginLine->Layers().Start(), ITEM::SOLID_T );

    if( !cnt )
        return true;

    for( auto j : joints )
        {
        if ( j->Net() == aOriginLine->Net() )
            continue;

        if ( encPoly.PointInside2( j->Pos() ) )
            {
            bool falsePositive = false;
            for( int k = 0; k < encPoly.PointCount(); k++)
                if(encPoly.CPoint(k) == j->Pos() )
                {
                    falsePositive = true;
                    break;
            }

            if( !falsePositive )
            {
                //dbg->AddPoint(j->Pos(), 5);
                return false;
            }
        }
    }

    return true;
}

bool OPTIMIZER::checkColliding( ITEM* aItem, bool aUpdateCache )
{
    CACHE_VISITOR v( aItem, m_world, m_collisionKindMask );

    return static_cast<bool>( m_world->CheckColliding( aItem ) );
}

void OPTIMIZER::ClearConstraints()
{
    for (auto c : m_constraints)
        delete c;
    m_constraints.clear();
}

void OPTIMIZER::AddConstraint ( OPT_CONSTRAINT *aConstraint )
    {
    m_constraints.push_back(aConstraint);
}

bool OPTIMIZER::checkConstraints(  int aVertex1, int aVertex2, LINE* aOriginLine, const SHAPE_LINE_CHAIN& aCurrentPath, const SHAPE_LINE_CHAIN& aReplacement )
{
    for( auto c: m_constraints)
    {
        if ( !c->Check( aVertex1, aVertex2, aOriginLine, aCurrentPath, aReplacement ) )
        {
            return false;
        }
    }

    return true;
}


bool OPTIMIZER::checkColliding( LINE* aLine, const SHAPE_LINE_CHAIN& aOptPath )
{
    LINE tmp( *aLine, aOptPath );

    return checkColliding( &tmp );
}


bool OPTIMIZER::mergeObtuse( LINE* aLine )
{
    SHAPE_LINE_CHAIN& line = aLine->Line();

    int step = line.PointCount() - 3;
    int iter = 0;
    int segs_pre = line.SegmentCount();

    if( step < 0 )
        return false;

    SHAPE_LINE_CHAIN current_path( line );

    while( 1 )
    {
        iter++;
        int n_segs = current_path.SegmentCount();
        int max_step = n_segs - 2;

        if( step > max_step )
            step = max_step;

        if( step < 2 )
        {
            line = current_path;
            return current_path.SegmentCount() < segs_pre;
        }

        bool found_anything = false;

        for( int n = 0; n < n_segs - step; n++ )
        {
            const SEG s1 = current_path.CSegment( n );
            const SEG s2 = current_path.CSegment( n + step );
            SEG s1opt, s2opt;

            if( DIRECTION_45( s1 ).IsObtuse( DIRECTION_45( s2 ) ) )
            {
                VECTOR2I ip = *s1.IntersectLines( s2 );

                if( s1.Distance( ip ) <= 1 || s2.Distance( ip ) <= 1 )
                {
                    s1opt = SEG( s1.A, ip );
                    s2opt = SEG( ip, s2.B );
                }
                else
                {
                    s1opt = SEG( s1.A, ip );
                    s2opt = SEG( ip, s2.B );
                }

                if( DIRECTION_45( s1opt ).IsObtuse( DIRECTION_45( s2opt ) ) )
                {
                    SHAPE_LINE_CHAIN opt_path;
                    opt_path.Append( s1opt.A );
                    opt_path.Append( s1opt.B );
                    opt_path.Append( s2opt.B );

                    LINE opt_track( *aLine, opt_path );

                    if( !checkColliding( &opt_track ) )
                    {
                        current_path.Replace( s1.Index() + 1, s2.Index(), ip );
                        // removeCachedSegments(aLine, s1.Index(), s2.Index());
                        n_segs = current_path.SegmentCount();
                        found_anything = true;
                        break;
                    }
                }
            }
         }

        if( !found_anything )
        {
            if( step <= 2 )
            {
                line = current_path;
                return line.SegmentCount() < segs_pre;
            }

            step--;
        }
    }

    return line.SegmentCount() < segs_pre;
}


bool OPTIMIZER::mergeFull( LINE* aLine )
{
    SHAPE_LINE_CHAIN& line = aLine->Line();
    int step = line.SegmentCount() - 1;

    int segs_pre = line.SegmentCount();

    line.Simplify();

    if( step < 0 )
        return false;

    SHAPE_LINE_CHAIN current_path( line );

    while( 1 )
    {
        int n_segs = current_path.SegmentCount();
        int max_step = n_segs - 2;

        if( step > max_step )
            step = max_step;

        if( step < 1 )
            break;

        bool found_anything = mergeStep( aLine, current_path, step );

        if( !found_anything )
            step--;

        if( !step )
            break;
    }

    aLine->SetShape( current_path );

    return current_path.SegmentCount() < segs_pre;
}


bool OPTIMIZER::Optimize( LINE* aLine, LINE* aResult )
{
    if( !aResult )
        aResult = aLine;
    else
    {
        *aResult = *aLine;
        aResult->ClearLinks();
    }

    m_keepPostures = false;

    bool rv = false;

    if( m_effortLevel & MERGE_SEGMENTS )
        rv |= mergeFull( aResult );

    if( m_effortLevel & MERGE_OBTUSE )
        rv |= mergeObtuse( aResult );

    if( m_effortLevel & SMART_PADS )
        rv |= runSmartPads( aResult );

    if( m_effortLevel & FANOUT_CLEANUP )
        rv |= fanoutCleanup( aResult );

    return rv;
}


bool OPTIMIZER::mergeStep( LINE* aLine, SHAPE_LINE_CHAIN& aCurrentPath, int step )
{
    int n_segs = aCurrentPath.SegmentCount();

    int cost_orig = COST_ESTIMATOR::CornerCost( aCurrentPath );

    if( aLine->SegmentCount() < 2 )
        return false;

    DIRECTION_45 orig_start( aLine->CSegment( 0 ) );
    DIRECTION_45 orig_end( aLine->CSegment( -1 ) );


    for( int n = 0; n < n_segs - step; n++ )
    {
        // Do not attempt to merge false segments that are part of an arc
        if( aCurrentPath.isArc( n ) || aCurrentPath.isArc( n + step ) )
            continue;

        const SEG s1    = aCurrentPath.CSegment( n );
        const SEG s2    = aCurrentPath.CSegment( n + step );

        SHAPE_LINE_CHAIN path[2];
        SHAPE_LINE_CHAIN* picked = NULL;
        int cost[2];

        for( int i = 0; i < 2; i++ )
        {
            SHAPE_LINE_CHAIN bypass = DIRECTION_45().BuildInitialTrace( s1.A, s2.B, i );
            cost[i] = INT_MAX;


            bool ok = false;
            if ( !checkColliding( aLine, bypass ) )
            {
                ok = checkConstraints ( n, n + step + 1, aLine, aCurrentPath, bypass );
            }

            if( ok )
            {
                path[i] = aCurrentPath;
                path[i].Replace( s1.Index(), s2.Index(), bypass );
                path[i].Simplify();
                cost[i] = COST_ESTIMATOR::CornerCost( path[i] );
            }
        }

        if( cost[0] < cost_orig && cost[0] < cost[1] )
            picked = &path[0];
        else if( cost[1] < cost_orig )
            picked = &path[1];

        if( picked )
        {
            n_segs = aCurrentPath.SegmentCount();
            aCurrentPath = *picked;
            return true;
        }
    }

    return false;
}


OPTIMIZER::BREAKOUT_LIST OPTIMIZER::circleBreakouts( int aWidth,
        const SHAPE* aShape, bool aPermitDiagonal ) const
{
    BREAKOUT_LIST breakouts;

    for( int angle = 0; angle < 360; angle += 45 )
    {
        const SHAPE_CIRCLE* cir = static_cast<const SHAPE_CIRCLE*>( aShape );
        SHAPE_LINE_CHAIN l;
        VECTOR2I p0 = cir->GetCenter();
        VECTOR2I v0( cir->GetRadius() * M_SQRT2, 0 );
        l.Append( p0 );
        l.Append( p0 + v0.Rotate( angle * M_PI / 180.0 ) );
        breakouts.push_back( l );
    }

    return breakouts;
}


OPTIMIZER::BREAKOUT_LIST OPTIMIZER::customBreakouts( int aWidth,
        const ITEM* aItem, bool aPermitDiagonal ) const
{
    BREAKOUT_LIST breakouts;
    const SHAPE_SIMPLE* convex = static_cast<const SHAPE_SIMPLE*>( aItem->Shape() );

    BOX2I bbox = convex->BBox( 0 );
    VECTOR2I p0 = static_cast<const SOLID*>( aItem )->Pos();
    // must be large enough to guarantee intersecting the convex polygon
    int length = std::max( bbox.GetWidth(), bbox.GetHeight() ) / 2 + 5;

    for( int angle = 0; angle < 360; angle += ( aPermitDiagonal ? 45 : 90 ) )
    {
        SHAPE_LINE_CHAIN l;
        VECTOR2I v0( p0 + VECTOR2I( length, 0 ).Rotate( angle * M_PI / 180.0 ) );
        SHAPE_LINE_CHAIN::INTERSECTIONS intersections;
        int n = convex->Vertices().Intersect( SEG( p0, v0 ), intersections );
        // if n == 1 intersected a segment
        // if n == 2 intersected the common point of 2 segments
        // n == 0 can not happen I think, but...
        if( n > 0 )
        {
            l.Append( p0 );

            // for a breakout distance relative to the distance between
            // center and polygon edge
            //l.Append( intersections[0].p + (v0 - p0).Resize( (intersections[0].p - p0).EuclideanNorm() * 0.4 ) );

            // for an absolute breakout distance, e.g. 0.1 mm
            //l.Append( intersections[0].p + (v0 - p0).Resize( 100000 ) );

            // for the breakout right on the polygon edge
            l.Append( intersections[0].p );

            breakouts.push_back( l );
        }
    }

    return breakouts;
}


OPTIMIZER::BREAKOUT_LIST OPTIMIZER::rectBreakouts( int aWidth,
        const SHAPE* aShape, bool aPermitDiagonal ) const
{
    const SHAPE_RECT* rect = static_cast<const SHAPE_RECT*>(aShape);
    VECTOR2I s = rect->GetSize();
    VECTOR2I c = rect->GetPosition() + VECTOR2I( s.x / 2, s.y / 2 );
    BREAKOUT_LIST breakouts;

    VECTOR2I d_offset;

    d_offset.x = ( s.x > s.y ) ? ( s.x - s.y ) / 2 : 0;
    d_offset.y = ( s.x < s.y ) ? ( s.y - s.x ) / 2 : 0;

    VECTOR2I d_vert  = VECTOR2I( 0, s.y / 2 + aWidth );
    VECTOR2I d_horiz = VECTOR2I( s.x / 2 + aWidth, 0 );

    breakouts.push_back( SHAPE_LINE_CHAIN( { c, c + d_horiz } ) );
    breakouts.push_back( SHAPE_LINE_CHAIN( { c, c - d_horiz } ) );
    breakouts.push_back( SHAPE_LINE_CHAIN( { c, c + d_vert } ) );
    breakouts.push_back( SHAPE_LINE_CHAIN( { c, c - d_vert } ) );

    if( aPermitDiagonal )
    {
        int l = aWidth + std::min( s.x, s.y ) / 2;
        VECTOR2I d_diag;

        if( s.x >= s.y )
        {
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c + d_offset, c + d_offset + VECTOR2I( l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c + d_offset, c + d_offset - VECTOR2I( -l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c - d_offset, c - d_offset + VECTOR2I( -l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c - d_offset, c - d_offset - VECTOR2I( l, l ) } ) );
        }
        else
        {
            // fixme: this could be done more efficiently
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c + d_offset, c + d_offset + VECTOR2I( l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c - d_offset, c - d_offset - VECTOR2I( -l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c + d_offset, c + d_offset + VECTOR2I( -l, l ) } ) );
            breakouts.push_back(
                    SHAPE_LINE_CHAIN( { c, c - d_offset, c - d_offset - VECTOR2I( l, l ) } ) );
        }
    }

    return breakouts;
}


OPTIMIZER::BREAKOUT_LIST OPTIMIZER::computeBreakouts( int aWidth,
        const ITEM* aItem, bool aPermitDiagonal ) const
{
    switch( aItem->Kind() )
    {
    case ITEM::VIA_T:
    {
        const VIA* via = static_cast<const VIA*>( aItem );
        return circleBreakouts( aWidth, via->Shape(), aPermitDiagonal );
    }

    case ITEM::SOLID_T:
    {
        const SHAPE* shape = aItem->Shape();

        switch( shape->Type() )
        {
        case SH_RECT:
            return rectBreakouts( aWidth, shape, aPermitDiagonal );

        case SH_SEGMENT:
        {
            const SHAPE_SEGMENT* seg = static_cast<const SHAPE_SEGMENT*> (shape);
            const SHAPE_RECT rect = ApproximateSegmentAsRect ( *seg );
            return rectBreakouts( aWidth, &rect, aPermitDiagonal );
        }

        case SH_CIRCLE:
            return circleBreakouts( aWidth, shape, aPermitDiagonal );

        case SH_SIMPLE:
            return customBreakouts( aWidth, aItem, aPermitDiagonal );

        default:
            break;
        }

        break;
    }

    default:
        break;
    }

    return BREAKOUT_LIST();
}


ITEM* OPTIMIZER::findPadOrVia( int aLayer, int aNet, const VECTOR2I& aP ) const
{
    JOINT* jt = m_world->FindJoint( aP, aLayer, aNet );

    if( !jt )
        return NULL;

    for( ITEM* item : jt->LinkList() )
    {
        if( item->OfKind( ITEM::VIA_T | ITEM::SOLID_T ) )
            return item;
    }

    return NULL;
}


int OPTIMIZER::smartPadsSingle( LINE* aLine, ITEM* aPad, bool aEnd, int aEndVertex )
{
    DIRECTION_45 dir;

    const int ForbiddenAngles = DIRECTION_45::ANG_ACUTE | DIRECTION_45::ANG_RIGHT |
                                DIRECTION_45::ANG_HALF_FULL | DIRECTION_45::ANG_UNDEFINED;

    typedef std::tuple<int, long long int, SHAPE_LINE_CHAIN> RtVariant;
    std::vector<RtVariant> variants;

    SOLID* solid = dyn_cast<SOLID*>( aPad );

    // don't do optimized connections for offset pads
    if( solid && solid->Offset() != VECTOR2I( 0, 0 ) )
        return -1;


    BREAKOUT_LIST    breakouts = computeBreakouts( aLine->Width(), aPad, true );
    SHAPE_LINE_CHAIN line = ( aEnd ? aLine->CLine().Reverse() : aLine->CLine() );
    int              p_end = std::min( aEndVertex, std::min( 3, line.PointCount() - 1 ) );

    // Start at 1 to find a potentially better breakout (0 is the pad connection)
    for( int p = 1; p <= p_end; p++ )
    {
        // If the line is contained inside the pad, don't optimize
        if( solid && solid->Shape() && !solid->Shape()->Collide(
                SEG( line.CPoint( 0 ), line.CPoint( p ) ), aLine->Width() / 2 ) )
            continue;

        for( SHAPE_LINE_CHAIN & breakout : breakouts ) {

            for( int diag = 0; diag < 2; diag++ )
            {
                SHAPE_LINE_CHAIN v;
                SHAPE_LINE_CHAIN connect = dir.BuildInitialTrace(
                        breakout.CPoint( -1 ), line.CPoint( p ), diag == 0 );

                DIRECTION_45 dir_bkout( breakout.CSegment( -1 ) );

                if(!connect.SegmentCount())
                    continue;

                int ang1 = dir_bkout.Angle( DIRECTION_45( connect.CSegment( 0 ) ) );

                if( ang1 & ForbiddenAngles )
                    continue;

                if( breakout.Length() > line.Length() )
                    continue;

                v = breakout;
                v.Append( connect );

                for( int i = p + 1; i < line.PointCount(); i++ )
                    v.Append( line.CPoint( i ) );

                LINE tmp( *aLine, v );
                int cc = tmp.CountCorners( ForbiddenAngles );

                if( cc == 0 )
                {
                    RtVariant vp;
                    std::get<0>( vp ) = p;
                    std::get<1>( vp ) = breakout.Length();
                    std::get<2>( vp ) = aEnd ? v.Reverse() : v;
                    std::get<2>( vp ).Simplify();
                    variants.push_back( vp );
                }
            }
        }
    }

    // We attempt to minimize the corner cost (minimizes the segments and types of corners)
    // but given two, equally valid costs, we want to pick the longer pad exit.  The logic
    // here is that if the pad is oblong, the track should not exit the shorter side and parallel
    // the pad but should follow the pad's preferential direction before exiting.
    // The baseline guess is to start with the existing line the user has drawn.
    int              min_cost   = COST_ESTIMATOR::CornerCost( *aLine );
    long long int    max_length = 0;
    bool             found      = false;
    int              p_best     = -1;
    SHAPE_LINE_CHAIN l_best;

    for( RtVariant& vp : variants )
    {
        LINE tmp( *aLine, std::get<2>( vp ) );
        int cost = COST_ESTIMATOR::CornerCost( std::get<2>( vp ) );
        long long int len = std::get<1>( vp );

        if( !checkColliding( &tmp ) )
        {
            if( cost < min_cost || ( cost == min_cost && len > max_length ) )
            {
                l_best = std::get<2>( vp );
                p_best = std::get<0>( vp );
                found  = true;

                if( cost <= min_cost )
                    max_length = std::max<int>( len, max_length );

                min_cost = std::min( cost, min_cost );
            }
        }
    }

    if( found )
    {
        aLine->SetShape( l_best );
        return p_best;
    }

    return -1;
}

bool OPTIMIZER::runSmartPads( LINE* aLine )
{
    SHAPE_LINE_CHAIN& line = aLine->Line();

    if( line.PointCount() < 3 )
        return false;

    VECTOR2I p_start = line.CPoint( 0 ), p_end = line.CPoint( -1 );

    ITEM* startPad = findPadOrVia( aLine->Layer(), aLine->Net(), p_start );
    ITEM* endPad = findPadOrVia( aLine->Layer(), aLine->Net(), p_end );

    int vtx = -1;

    if( startPad )
        vtx = smartPadsSingle( aLine, startPad, false, 3 );

    if( endPad )
        smartPadsSingle( aLine, endPad, true,
                         vtx < 0 ? line.PointCount() - 1 : line.PointCount() - 1 - vtx );

    aLine->Line().Simplify();

    return true;
}


bool OPTIMIZER::Optimize( LINE* aLine, int aEffortLevel, NODE* aWorld, const VECTOR2I aV )
{
    OPTIMIZER opt( aWorld );

    g_dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();

    opt.SetEffortLevel( aEffortLevel );
    opt.SetCollisionMask( -1 );

    if ( aEffortLevel & PRESERVE_VERTEX )
    {
        auto c = new PRESERVE_VERTEX_CONSTRAINT( aWorld, aV );
        opt.AddConstraint( c );
    }

    if ( aEffortLevel & KEEP_TOPOLOGY )
    {
        auto c = new KEEP_TOPOLOGY_CONSTRAINT( aWorld );
        opt.AddConstraint( c );
    }

    if ( aEffortLevel & RESTRICT_AREA )
    {
        //auto c = new AREA_CONSTRAINT( aWorld );
        //opt.AddConstraint( c );
    }


    return opt.Optimize( aLine );
}


bool OPTIMIZER::fanoutCleanup( LINE* aLine )
{
    if( aLine->PointCount() < 3 )
        return false;

    VECTOR2I p_start = aLine->CPoint( 0 ), p_end = aLine->CPoint( -1 );

    ITEM* startPad = findPadOrVia( aLine->Layer(), aLine->Net(), p_start );
    ITEM* endPad = findPadOrVia( aLine->Layer(), aLine->Net(), p_end );

    int thr = aLine->Width() * 10;
    int len = aLine->CLine().Length();

    if( !startPad )
        return false;

    bool startMatch = startPad->OfKind( ITEM::VIA_T | ITEM::SOLID_T );
    bool endMatch = false;

    if(endPad)
    {
        endMatch = endPad->OfKind( ITEM::VIA_T | ITEM::SOLID_T );
    }
    else
    {
        endMatch = aLine->EndsWithVia();
    }

    if( startMatch && endMatch && len < thr )
    {
        for( int i = 0; i < 2; i++ )
        {
            SHAPE_LINE_CHAIN l2 = DIRECTION_45().BuildInitialTrace( p_start, p_end, i );
            LINE repl;
            repl = LINE( *aLine, l2 );

            if( !m_world->CheckColliding( &repl ) )
            {
                aLine->SetShape( repl.CLine() );
                return true;
            }
        }
    }

    return false;
}


int findCoupledVertices( const VECTOR2I& aVertex, const SEG& aOrigSeg, const SHAPE_LINE_CHAIN& aCoupled, DIFF_PAIR* aPair, int* aIndices )
{
    int count = 0;
    for ( int i = 0; i < aCoupled.SegmentCount(); i++ )
    {
        SEG s = aCoupled.CSegment( i );
        VECTOR2I projOverCoupled = s.LineProject ( aVertex );

        if( s.ApproxParallel ( aOrigSeg ) )
        {
            int64_t dist = ( projOverCoupled - aVertex ).EuclideanNorm() - aPair->Width();

            if( aPair->GapConstraint().Matches( dist ) )
            {
               *aIndices++ = i;
               count++;
            }
        }
    }

    return count;
}


bool verifyDpBypass( NODE* aNode, DIFF_PAIR* aPair, bool aRefIsP, const SHAPE_LINE_CHAIN& aNewRef, const SHAPE_LINE_CHAIN& aNewCoupled )
{
    LINE refLine ( aRefIsP ? aPair->PLine() : aPair->NLine(), aNewRef );
    LINE coupledLine ( aRefIsP ? aPair->NLine() : aPair->PLine(), aNewCoupled );

    if( aNode->CheckColliding( &refLine, &coupledLine, ITEM::ANY_T, aPair->Gap() - 10 ) )
        return false;

    if( aNode->CheckColliding ( &refLine ) )
        return false;

    if( aNode->CheckColliding ( &coupledLine ) )
        return false;

    return true;
}


bool coupledBypass( NODE* aNode, DIFF_PAIR* aPair, bool aRefIsP, const SHAPE_LINE_CHAIN& aRef, const SHAPE_LINE_CHAIN& aRefBypass, const SHAPE_LINE_CHAIN& aCoupled, SHAPE_LINE_CHAIN& aNewCoupled )
{
    int vStartIdx[1024]; // fixme: possible overflow

    int nStarts = findCoupledVertices( aRefBypass.CPoint( 0 ), aRefBypass.CSegment( 0 ), aCoupled, aPair, vStartIdx );
    DIRECTION_45 dir( aRefBypass.CSegment( 0 ) );

    int64_t bestLength = -1;
    bool found = false;
    SHAPE_LINE_CHAIN bestBypass;
    int si, ei;

    for( int i=0; i< nStarts; i++ )
    {
        for( int j = 1; j < aCoupled.PointCount() - 1; j++ )
        {
            int delta = std::abs ( vStartIdx[i] - j );

            if( delta > 1 )
            {
                const VECTOR2I& vs = aCoupled.CPoint( vStartIdx[i] );
                SHAPE_LINE_CHAIN bypass = dir.BuildInitialTrace( vs, aCoupled.CPoint(j), dir.IsDiagonal() );

                int64_t coupledLength = aPair->CoupledLength( aRef, bypass );

                SHAPE_LINE_CHAIN newCoupled = aCoupled;

                si = vStartIdx[i];
                ei = j;

                if(si < ei)
                    newCoupled.Replace( si, ei, bypass );
                else
                    newCoupled.Replace( ei, si, bypass.Reverse() );

                if(coupledLength > bestLength && verifyDpBypass( aNode, aPair, aRefIsP, aRef, newCoupled) )
                {
                    bestBypass = newCoupled;
                    bestLength = coupledLength;
                    found = true;
                }
            }
        }
    }


    if( found )
        aNewCoupled = bestBypass;

    return found;
}


bool checkDpColliding( NODE* aNode, DIFF_PAIR* aPair, bool aIsP, const SHAPE_LINE_CHAIN& aPath )
{
    LINE tmp ( aIsP ? aPair->PLine() : aPair->NLine(), aPath );

    return static_cast<bool>( aNode->CheckColliding( &tmp ) );
}


bool OPTIMIZER::mergeDpStep( DIFF_PAIR* aPair, bool aTryP, int step )
{
    int n = 1;

    SHAPE_LINE_CHAIN currentPath = aTryP ? aPair->CP() : aPair->CN();
    SHAPE_LINE_CHAIN coupledPath = aTryP ? aPair->CN() : aPair->CP();

    int n_segs = currentPath.SegmentCount() - 1;

    int64_t clenPre = aPair->CoupledLength( currentPath, coupledPath );
    int64_t budget = clenPre / 10; // fixme: come up with somethig more intelligent here...

    
    auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();


    while( n < n_segs - step )
    {
        const SEG s1    = currentPath.CSegment( n );
        const SEG s2    = currentPath.CSegment( n + step );

        DIRECTION_45 dir1( s1 );
        DIRECTION_45 dir2( s2 );

        dbg->AddPoint( s1.A, 4 );
        dbg->AddPoint( s2.B, 6 );

        if( dir1.IsObtuse( dir2 ) )
        {
            SHAPE_LINE_CHAIN bypass = DIRECTION_45().BuildInitialTrace( s1.A, s2.B, dir1.IsDiagonal() );
            SHAPE_LINE_CHAIN newRef;
            SHAPE_LINE_CHAIN newCoup;
            int64_t deltaCoupled = -1, deltaUni = -1;

            newRef = currentPath;
            newRef.Replace( s1.Index(), s2.Index(), bypass );


            deltaUni = aPair->CoupledLength ( newRef, coupledPath ) - clenPre + budget;

            if ( coupledBypass( m_world, aPair, aTryP, newRef, bypass, coupledPath, newCoup ) )
            {
                deltaCoupled = aPair->CoupledLength( newRef, newCoup ) - clenPre + budget;

                if( deltaCoupled >= 0 )
                {
                    newRef.Simplify();
                    newCoup.Simplify();

                    aPair->SetShape( newRef, newCoup, !aTryP );
                    return true;
                }
            }
            else if( deltaUni >= 0 &&  verifyDpBypass ( m_world, aPair, aTryP, newRef, coupledPath ) )
            {
                newRef.Simplify();
                coupledPath.Simplify();

                aPair->SetShape( newRef, coupledPath, !aTryP );
                return true;
            }
        }

        n++;
    }

    return false;
}


bool OPTIMIZER::mergeDpSegments( DIFF_PAIR* aPair )
{
    int step_p = aPair->CP().SegmentCount() - 2;
    int step_n = aPair->CN().SegmentCount() - 2;

    printf("MergeDPSegs: step %d %d\n", step_p, step_n);
    auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();

    while( 1 )
    {
        int n_segs_p = aPair->CP().SegmentCount();
        int n_segs_n = aPair->CN().SegmentCount();

        int max_step_p = n_segs_p - 2;
        int max_step_n = n_segs_n - 2;

        if( step_p > max_step_p )
            step_p = max_step_p;

        if( step_n > max_step_n )
            step_n = max_step_n;

        if( step_p < 1 && step_n < 1)
            break;

        bool found_anything_p = false;
        bool found_anything_n = false;

        dbg->Message(wxString::Format("mergeDpstep=%d n=%d", step_p, step_n, n_segs_p, n_segs_n ));
        dbg->BeginGroup("merge-dp-step");

        if( step_p > 1 )
            found_anything_p = mergeDpStep( aPair, true, step_p );

        if( step_n > 1 )
            found_anything_n = mergeDpStep( aPair, false, step_n );

        dbg->EndGroup();

        if( !found_anything_n && !found_anything_p )
        {
            step_n--;
            step_p--;
        }
    }
    return true;
}

static int projectVectorOnLineChain( const SHAPE_LINE_CHAIN& lc, VECTOR2I p0, RANGED_NUM<int> gap, int width, std::vector<VECTOR2I>& aOut )
{
    VECTOR2I::extended_type best_dist = VECTOR2I::ECOORD_MAX, dist;
    OPT_VECTOR2I rv;

    for(int i = 0; i < lc.SegmentCount(); i++)
    {
        auto s = lc.CSegment(i);
        VECTOR2I pp = s.LineProject( p0 );

        if( s.Distance( pp ) < 10000 ) // fixme: contains?
        {
            dist = (pp - p0 ).EuclideanNorm() - width;
            printf("dist %d gap %d\n", dist, gap);
            if( gap.Matches(dist) )
            {
                aOut.emplace_back( pp );
            }
        }
    }
    return aOut.size();
}

void diagonalizeGw( VECTOR2I pP, VECTOR2I pN, const SHAPE_LINE_CHAIN& lP, const SHAPE_LINE_CHAIN& lN, int gap )
{

}

void buildGatewaysForSide( DIFF_PAIR* aPair, const SHAPE_LINE_CHAIN& lA, const SHAPE_LINE_CHAIN& lB,
        std::vector<DP_GATEWAY>& gws, bool swap )
{
    auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();
    for( int i = 0; i < lA.PointCount(); i++ )
    {
        auto                  v = lA.CPoint( i );
        std::vector<VECTOR2I> candidates;
        projectVectorOnLineChain( lB, v, aPair->GapConstraint(), aPair->Width(), candidates );


        char str[128];
        sprintf( str, "v%d", i );

        for( const auto& prj : candidates )
        {
            dbg->Message(wxString::Format("v%d %d %d %d %d\n", i, v.x, v.y, prj.x, prj.y ));
            dbg->BeginGroup( str );
            dbg->AddPoint( v, 5 );
            dbg->AddPoint( prj, 4 );
            dbg->AddSegment( SEG( v, prj ), 2 );
            if( swap )
                gws.emplace_back( prj, v, false );
            else
                gws.emplace_back( v, prj, false );

            dbg->EndGroup();
        }
    }
}




bool verifyDpBypass( NODE* aNode, DIFF_PAIR* aPair, const SHAPE_LINE_CHAIN& aNewP, const SHAPE_LINE_CHAIN& aNewN )
{
    LINE pLine ( aPair->PLine(), aNewP );
    LINE nLine ( aPair->NLine(), aNewN );

    if( aNode->CheckColliding( &pLine, &nLine, ITEM::ANY_T, aPair->Gap() - 10 ) )
        return false;

   // if( aNode->CheckColliding ( &pLine ) )
        //return false;

    //if( aNode->CheckColliding ( &nLine ) )
      //  return false;

    return true;
}

void OPTIMIZER::buildGatewaysForDp( DIFF_PAIR* aPair,  std::vector<DP_GATEWAY>& gws )
{
    auto lp = aPair->CP();
    auto ln = aPair->CN();
    buildGatewaysForSide( aPair, lp, ln, gws, false );
    buildGatewaysForSide( aPair, ln, lp, gws, true );

    for( auto& gw : gws )
    {
        int d = lp.PathLength( gw.AnchorP(), 3 );
        gw.SetOriginDistance( d );

        lp.Split( gw.AnchorP() );
        ln.Split( gw.AnchorP() );
    }

    std::sort( gws.begin(), gws.end(), [] ( const DP_GATEWAY&a, const DP_GATEWAY&b ){ return a.OriginDistance() < b.OriginDistance(); } );

    for( auto& gw : gws )
    {
        int d = gw.OriginDistance();
        printf("GW odis %d\n", d );
    }

    int step = gws.size() - 1;
    auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();

    dbg->BeginGroup("mergegws\n");

    printf("Step %d\n", step );

    while( step > 1 )
    {
        bool diagonal = true;
        for(int i = 0; i < gws.size() - step; i++)
        {
            for(int diag = 0; diag < 2; diag++ )
            {
                const auto& gwA = gws[i];
                const auto& gwB = gws[i+step];
                auto bypP = DIRECTION_45().BuildInitialTrace( gwA.AnchorP(), gwB.AnchorP(), diag );
                auto bypN = DIRECTION_45().BuildInitialTrace( gwA.AnchorN(), gwB.AnchorN(), diag );

                if(verifyDpBypass( m_world, aPair, bypP, bypN ))
                {
                    dbg->AddLine(bypP, 4, 10000 );
                    dbg->AddLine(bypN, 5, 10000 );
                }
            }
        }

        step--;
    }
    dbg->EndGroup();

}

bool OPTIMIZER::Optimize( DIFF_PAIR* aPair )
{
    std::vector<DP_GATEWAY> gateways;
    aPair->ClearLinks();
    buildGatewaysForDp( aPair, gateways );

    

    return false; //mergeDpSegments( aPair );
}

static int64_t shovedArea( const SHAPE_LINE_CHAIN& aOld, const SHAPE_LINE_CHAIN& aNew )
{
    int64_t area = 0;
    const int oc = aOld.PointCount();
    const int nc = aNew.PointCount();
    const int total = oc + nc;

    for(int i = 0; i < total; i++)
    {
        int i_next = (i + 1 == total ? 0 : i + 1);

        const VECTOR2I &v0 = ( i < oc ? aOld.CPoint(i) : aNew.CPoint( nc - 1 - (i - oc) ) );
        const VECTOR2I &v1 = ( i_next < oc ? aOld.CPoint ( i_next ) : aNew.CPoint( nc - 1 - (i_next - oc) ) );
        area += -(int64_t) v0.y * v1.x + (int64_t) v0.x * v1.y;
    }

    return std::abs(area / 2);
}

bool tightenSegment( bool dir, NODE *aNode, const LINE& cur,
                     const SHAPE_LINE_CHAIN& in, SHAPE_LINE_CHAIN& out )
{
    SEG a = in.CSegment(0);
    SEG center = in.CSegment(1);
    SEG b = in.CSegment(2);

    DIRECTION_45 dirA ( a );
    DIRECTION_45 dirCenter ( center );
    DIRECTION_45 dirB ( b );

    if (!dirA.IsObtuse( dirCenter) || !dirCenter.IsObtuse(dirB))
        return false;

    //VECTOR2I perp = (center.B - center.A).Perpendicular();
    VECTOR2I guideA, guideB ;

    SEG guide;
    int initial;

    //auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();
    if ( dirA.Angle ( dirB ) != DIRECTION_45::ANG_RIGHT )
        return false;

    {
        //auto rC = *a.IntersectLines( b );
      //  dbg->AddSegment ( SEG( center.A, rC ), 1 );
      //  dbg->AddSegment ( SEG( center.B, rC ), 2 );
        /*
        auto perp = dirCenter.Left().Left();

        SEG sperp ( center.A, center.A + perp.ToVector() );

        auto vpc = sperp.LineProject( rC );
        auto vpa = sperp.LineProject( a.A );
        auto vpb = sperp.LineProject( b.B );

        auto da = (vpc - vpa).EuclideanNorm();
        auto db = (vpc - vpb).EuclideanNorm();

        auto vp = (da < db) ? vpa : vpb;
        dbg->AddSegment ( SEG( vpc, vp ), 5 );


        guide = SEG ( vpc, vp );*/


    }

    int da = a.Length();
    int db = b.Length();

    if ( da < db )
        guide = a;
    else
        guide = b;


    initial = guide.Length();

    int step = initial;
    int current = step;
    //printf("step %d\n", step);
    SHAPE_LINE_CHAIN snew;

    while (step > 1)
    {
        LINE l ( cur );


        //printf("current %d l %d\n", current, guide.Length() );
        snew.Clear();
        snew.Append( a.A );
        snew.Append( a.B + (a.A - a.B).Resize( current ) );
        snew.Append( b.A + (b.B - b.A).Resize( current ) );
        snew.Append( b.B );

        step /= 2;

        l.SetShape(snew);
        if( aNode->CheckColliding(&l) )
        {
            current -= step;
        } else if ( current + step >= initial ) {
            current = initial;
        } else {
            current += step;
        }


        //dbg->AddSegment ( SEG( center.A ,  a.LineProject( center.A + gr ) ), 3 );
        //dbg->AddSegment ( SEG( center.A ,  center.A + guideA  ), 3 );
        //dbg->AddSegment ( SEG( center.B , center.B + guideB ), 4 );


        if ( current == initial )
            break;


    }
    out = snew;

    //dbg->AddLine ( snew, 3, 100000 );

    return true;
}

void Tighten( NODE *aNode, SHAPE_LINE_CHAIN& aOldLine, LINE& aNewLine, LINE& aOptimized )
{
    LINE tmp;



    if ( aNewLine.SegmentCount() < 3 )
        return;

    SHAPE_LINE_CHAIN current ( aNewLine.CLine() );

    for (int step = 0; step < 3; step++)
    {
        current.Simplify();

        for ( int i = 0; i <= current.SegmentCount() - 3; i++)
        {
            SHAPE_LINE_CHAIN l_in, l_out;

            l_in = current.Slice(i, i+3);
            for (int dir = 0; dir < 1; dir++)
            {
                if(  tightenSegment( dir ? true : false, aNode, aNewLine, l_in, l_out ) )
                {
                    SHAPE_LINE_CHAIN opt = current;
                    opt.Replace(i, i+3, l_out);
                    auto optArea = std::abs(shovedArea( aOldLine, opt ));
                    auto prevArea = std::abs(shovedArea( aOldLine, current ));

                    if( optArea < prevArea )
                    {
                        current = opt;
                    }
                    break;
                }

            }
        }

    }

    aOptimized = LINE(aNewLine, current);

    //auto dbg = ROUTER::GetInstance()->GetInterface()->GetDebugDecorator();
    //dbg->AddLine ( current, 4, 100000 );
}


std::vector<LINK_HOLDER*> pruneDpSegments( std::vector<DIFF_PAIR*>& aPairs, std::vector<LINE>& origLines )
{
    std::vector<LINK_HOLDER*> queue;
    std::set<DIFF_PAIR*> processedPairs;

    for (auto& line : origLines)
    {
        bool found= false;

        for( auto& diffPair: aPairs )
        {
            int i = 0;

            for( auto link : diffPair->Links() )
            {
                if ( diffPair->ContainsLink( link ) )
                    found = true;
                
                if( found && processedPairs.find( diffPair ) == processedPairs.end() )
                {
                    queue.push_back( diffPair );
                    processedPairs.insert(diffPair);
                }

                if( found )
                    break;
            }
            if(found)
                break;
        }

        if(!found)
            queue.push_back( &line );
    }

    

    return queue;
}

void OPTIMIZER::OptimizeLineQueue( std::vector<LINE>& aLines )
{
    
    int optFlags = MERGE_SEGMENTS;
    int n_passes = 1;

    auto router = ROUTER::GetInstance();
    auto resolver = router->GetInterface()->GetRuleResolver();
    auto dbg = router->GetInterface()->GetDebugDecorator();
    //)->AddLine(optimized.CLine(), 6, 100000 );

    if( router->Settings().SmartPads() )
        optFlags |= OPTIMIZER::SMART_PADS;

    std::vector<LINK_HOLDER*> queue;
    std::vector<DIFF_PAIR*> diffPairs;

    TOPOLOGY topo(m_world);

    // find differential pairs & their associated constraints and prune them from the line queue
    for ( auto &line : aLines )
        queue.push_back( &line );
    
    for( auto &line : aLines )
    {

        //printf("printf scan L %d %d %d\n", line.Net(), resolver->DpBelongsToDiffPair( &line),  line.SegmentCount() );
        if( resolver->DpBelongsToDiffPair( &line ) && line.LinkCount() > 0 && line.SegmentCount() > 0 )
        {
            DIFF_PAIR *dp = new DIFF_PAIR;

            if( topo.AssembleDiffPair( &line, *dp, true, queue ) )
            {
         //       printf("DP [%d %d segs %d %d]\n", dp->NetN(), dp->NetP(), dp->CP().SegmentCount(), dp->CN().SegmentCount() );
                diffPairs.push_back(dp);
            } else {
                delete dp;
            }
        }
    };

    queue = pruneDpSegments( diffPairs, aLines );

    for( auto item :queue )
    {
        dbg->Message( wxString::Format( "---> optq %s %p %x\n", item->KindStr().c_str(), item, item->Marker() ) );
        
    }

    SetEffortLevel( optFlags );
    SetCollisionMask( ITEM::ANY_T );


  #if 1
    for( int pass = 0; pass < n_passes; pass++ )
    {
        std::reverse( queue.begin(), queue.end() );

        for( auto qitem : queue )
        {
            if( qitem->Marker() & MK_HEAD )
                continue;

            if( auto origLine = dyn_cast<LINE*>( qitem ) )
            {
                LINE optimized(*origLine);
                optimized.ClearLinks();
              //  printf("Fixme: optimze LINE LC %d\n\n", optimized.LinkCount() );

                if( resolver->DpBelongsToDiffPair( qitem ) )
                    continue;
                
               
                
                if( Optimize( origLine, &optimized ) )
                {
                    dbg->AddLine(origLine->CLine(), 3, 30000 ,"orig-line" );
                    dbg->AddLine(optimized.CLine(), 6, 30000 ,"opt-line" );
                    
                    m_world->Remove( origLine );
                    //printf("Fixme: optimze2 LINE LC %d\n\n", optimized.LinkCount() );


                    m_world->Add( optimized );
                }
            } else if ( auto origDp = dyn_cast<DIFF_PAIR*>( qitem ) )
            {
                //printf("Fixme: optimze DP\n");
                DIFF_PAIR optimized( *origDp );
                if ( Optimize( &optimized ))
                {
                    dbg->AddLine(origDp->PLine().CLine(),3,30000,"dp-orig-line-p");
                    dbg->AddLine(origDp->NLine().CLine(),3,30000,"dp-orig-line-n");
                    m_world->Remove( origDp->PLine() );
                    m_world->Remove( origDp->NLine() );
                    dbg->AddLine(optimized.PLine().CLine(),3,30000,"dp-opt-line-p");
                    dbg->AddLine(optimized.NLine().CLine(),3,30000,"dp-opt-line-n");
                    m_world->Add( optimized.PLine() );
                    m_world->Add( optimized.NLine() );
                }
            }
            
        }
    }
    #endif
}

}

