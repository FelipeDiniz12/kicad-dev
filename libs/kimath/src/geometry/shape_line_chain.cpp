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

#include <algorithm>
#include <limits.h>          // for INT_MAX
#include <math.h>            // for hypot
#include <string>            // for basic_string

#include <clipper.hpp>
#include <geometry/seg.h>    // for SEG, OPT_VECTOR2I
#include <geometry/shape_line_chain.h>
#include <math/box2.h>       // for BOX2I
#include <math/util.h>  // for rescale
#include <math/vector2d.h>   // for VECTOR2, VECTOR2I

class SHAPE;

SHAPE_LINE_CHAIN::SHAPE_LINE_CHAIN( const std::vector<int>& aV)
    : SHAPE( SH_LINE_CHAIN ), m_closed( false ), m_width( 0 )
{
    for(size_t i = 0; i < aV.size(); i+= 2 )
    {
        Append( aV[i], aV[i+1] );
    }
}

ClipperLib::Path SHAPE_LINE_CHAIN::convertToClipper( bool aRequiredOrientation ) const
{
    ClipperLib::Path c_path;

    for( int i = 0; i < PointCount(); i++ )
    {
        const VECTOR2I& vertex = CPoint( i );
        c_path.push_back( ClipperLib::IntPoint( vertex.x, vertex.y ) );
    }

    if( Orientation( c_path ) != aRequiredOrientation )
        ReversePath( c_path );

    return c_path;
}


//TODO(SH): Adjust this into two functions: one to convert and one to split the arc into two arcs
void SHAPE_LINE_CHAIN::convertArc( ssize_t aArcIndex )
{
    if( aArcIndex < 0 )
        aArcIndex += m_arcs.size();

    if( aArcIndex >= static_cast<ssize_t>( m_arcs.size() ) )
        return;

    // Clear the shapes references
    for( auto& sh : m_shapes )
    {
        if( sh == aArcIndex )
            sh = SHAPE_IS_PT;

        if( sh > aArcIndex )
            --sh;
    }

    m_arcs.erase( m_arcs.begin() + aArcIndex );
}


bool SHAPE_LINE_CHAIN::Collide( const VECTOR2I& aP, int aClearance ) const
{
    // fixme: ugly!
    SEG s( aP, aP );
    return this->Collide( s, aClearance );
}


void SHAPE_LINE_CHAIN::Rotate( double aAngle, const VECTOR2I& aCenter )
{
    for( auto& pt : m_points )
    {
        pt -= aCenter;
        pt = pt.Rotate( aAngle );
        pt += aCenter;
    }

    for( auto& arc : m_arcs )
        arc.Rotate( aAngle, aCenter );
}


bool SHAPE_LINE_CHAIN::Collide( const SEG& aSeg, int aClearance ) const
{
    BOX2I box_a( aSeg.A, aSeg.B - aSeg.A );
    BOX2I::ecoord_type dist_sq = (BOX2I::ecoord_type) aClearance * aClearance;

    for( int i = 0; i < SegmentCount(); i++ )
    {
        const SEG& s = CSegment( i );
        BOX2I box_b( s.A, s.B - s.A );

        BOX2I::ecoord_type d = box_a.SquaredDistance( box_b );

        if( d < dist_sq )
        {
            if( s.Collide( aSeg, aClearance ) )
                return true;
        }
    }

    return false;
}


const SHAPE_LINE_CHAIN SHAPE_LINE_CHAIN::Reverse() const
{
    SHAPE_LINE_CHAIN a( *this );

    reverse( a.m_points.begin(), a.m_points.end() );
    reverse( a.m_shapes.begin(), a.m_shapes.end() );
    reverse( a.m_arcs.begin(), a.m_arcs.end() );

    for( auto& sh : a.m_shapes )
    {
        if( sh != SHAPE_IS_PT )
            sh = a.m_arcs.size() - sh - 1;
    }


    a.m_closed = m_closed;

    return a;
}


long long int SHAPE_LINE_CHAIN::Length() const
{
    long long int l = 0;

    for( int i = 0; i < SegmentCount(); i++ )
        l += CSegment( i ).Length();

    return l;
}


void SHAPE_LINE_CHAIN::Mirror( bool aX, bool aY, const VECTOR2I& aRef )
{
    for( auto& pt : m_points )
    {
        if( aX )
            pt.x = -pt.x + 2 * aRef.x;

        if( aY )
            pt.y = -pt.y + 2 * aRef.y;
    }

    for( auto& arc : m_arcs )
        arc.Mirror( aX, aY, aRef );
}


void SHAPE_LINE_CHAIN::Replace( int aStartIndex, int aEndIndex, const VECTOR2I& aP )
{
    if( aEndIndex < 0 )
        aEndIndex += PointCount();

    if( aStartIndex < 0 )
        aStartIndex += PointCount();

    aEndIndex = std::min( aEndIndex, PointCount() - 1 );

    // N.B. This works because convertArc changes m_shapes on the first run
    for( int ind = aStartIndex; ind <= aEndIndex; ind++ )
    {
        if( m_shapes[ind] != SHAPE_IS_PT )
            convertArc( ind );
    }

    if( aStartIndex == aEndIndex )
        m_points[aStartIndex] = aP;
    else
    {
        m_points.erase( m_points.begin() + aStartIndex + 1, m_points.begin() + aEndIndex + 1 );
        m_points[aStartIndex] = aP;

        m_shapes.erase( m_shapes.begin() + aStartIndex + 1, m_shapes.begin() + aEndIndex + 1 );
    }

    assert( m_shapes.size() == m_points.size() );
}


void SHAPE_LINE_CHAIN::Replace( int aStartIndex, int aEndIndex, const SHAPE_LINE_CHAIN& aLine )
{
    if( aEndIndex < 0 )
        aEndIndex += PointCount();

    if( aStartIndex < 0 )
        aStartIndex += PointCount();

    Remove( aStartIndex, aEndIndex );

    // The total new arcs index is added to the new arc indices
    size_t prev_arc_count = m_arcs.size();
    auto   new_shapes = aLine.m_shapes;

    for( auto& shape : new_shapes )
        shape += prev_arc_count;

    m_shapes.insert( m_shapes.begin() + aStartIndex, new_shapes.begin(), new_shapes.end() );
    m_points.insert( m_points.begin() + aStartIndex, aLine.m_points.begin(), aLine.m_points.end() );
    m_arcs.insert( m_arcs.end(), aLine.m_arcs.begin(), aLine.m_arcs.end() );

    assert( m_shapes.size() == m_points.size() );
}


void SHAPE_LINE_CHAIN::Remove( int aStartIndex, int aEndIndex )
{
    assert( m_shapes.size() == m_points.size() );
    if( aEndIndex < 0 )
        aEndIndex += PointCount();

    if( aStartIndex < 0 )
        aStartIndex += PointCount();

    if( aStartIndex >= PointCount() )
        return;

    aEndIndex = std::min( aEndIndex, PointCount() );
    std::set<size_t> extra_arcs;

    // Remove any overlapping arcs in the point range
    for( int i = aStartIndex; i < aEndIndex; i++ )
    {
        if( m_shapes[i] != SHAPE_IS_PT )
            extra_arcs.insert( m_shapes[i] );
    }

    for( auto arc : extra_arcs )
        convertArc( arc );

    m_shapes.erase( m_shapes.begin() + aStartIndex, m_shapes.begin() + aEndIndex + 1 );
    m_points.erase( m_points.begin() + aStartIndex, m_points.begin() + aEndIndex + 1 );
    assert( m_shapes.size() == m_points.size() );
}


int SHAPE_LINE_CHAIN::Distance( const VECTOR2I& aP, bool aOutlineOnly ) const
{
    int d = INT_MAX;

    if( IsClosed() && PointInside( aP ) && !aOutlineOnly )
        return 0;

    for( int s = 0; s < SegmentCount(); s++ )
        d = std::min( d, CSegment( s ).Distance( aP ) );

    return d;
}


int SHAPE_LINE_CHAIN::Split( const VECTOR2I& aP )
{
    int ii = -1;
    int min_dist = 2;

    int found_index = Find( aP );

    for( int s = 0; s < SegmentCount(); s++ )
    {
        const SEG seg = CSegment( s );
        int dist = seg.Distance( aP );

        // make sure we are not producing a 'slightly concave' primitive. This might happen
        // if aP lies very close to one of already existing points.
        if( dist < min_dist && seg.A != aP && seg.B != aP )
        {
            min_dist = dist;
            if( found_index < 0 )
                ii = s;
            else if( s < found_index )
                ii = s;
        }
    }

    if( ii < 0 )
        ii = found_index;

    if( ii >= 0 )
    {
        m_points.insert( m_points.begin() + ii + 1, aP );
        m_shapes.insert( m_shapes.begin() + ii + 1, ssize_t( SHAPE_IS_PT ) );

        return ii + 1;
    }

    return -1;
}


int SHAPE_LINE_CHAIN::Find( const VECTOR2I& aP ) const
{
    for( int s = 0; s < PointCount(); s++ )
        if( CPoint( s ) == aP )
            return s;

    return -1;
}


int SHAPE_LINE_CHAIN::FindSegment( const VECTOR2I& aP ) const
{
    for( int s = 0; s < SegmentCount(); s++ )
        if( CSegment( s ).Distance( aP ) <= 1 )
            return s;

    return -1;
}


const SHAPE_LINE_CHAIN SHAPE_LINE_CHAIN::Slice( int aStartIndex, int aEndIndex ) const
{
    SHAPE_LINE_CHAIN rv;

    if( aEndIndex < 0 )
        aEndIndex += PointCount();

    if( aStartIndex < 0 )
        aStartIndex += PointCount();

    for( int i = aStartIndex; i <= aEndIndex && static_cast<size_t>( i ) < m_points.size(); i++ )
        rv.Append( m_points[i] );

    return rv;
}


void SHAPE_LINE_CHAIN::Append( const SHAPE_LINE_CHAIN& aOtherLine )
{
    assert( m_shapes.size() == m_points.size() );

    if( aOtherLine.PointCount() == 0 )
        return;

    else if( PointCount() == 0 || aOtherLine.CPoint( 0 ) != CPoint( -1 ) )
    {
        const VECTOR2I p = aOtherLine.CPoint( 0 );
        m_points.push_back( p );
        m_shapes.push_back( ssize_t( SHAPE_IS_PT ) );
        m_bbox.Merge( p );
    }

    size_t num_arcs = m_arcs.size();
    m_arcs.insert( m_arcs.end(), aOtherLine.m_arcs.begin(), aOtherLine.m_arcs.end() );

    for( int i = 1; i < aOtherLine.PointCount(); i++ )
    {
        const VECTOR2I p = aOtherLine.CPoint( i );
        m_points.push_back( p );

        ssize_t arcIndex = aOtherLine.ArcIndex( i );

        if( arcIndex != ssize_t( SHAPE_IS_PT ) )
            m_shapes.push_back( num_arcs + arcIndex );
        else
            m_shapes.push_back( ssize_t( SHAPE_IS_PT ) );

        m_bbox.Merge( p );
    }

    assert( m_shapes.size() == m_points.size() );
}


void SHAPE_LINE_CHAIN::Append( const SHAPE_ARC& aArc )
{
    auto& chain = aArc.ConvertToPolyline();

    for( auto& pt : chain.CPoints() )
    {
        m_points.push_back( pt );
        m_shapes.push_back( m_arcs.size() );
    }

    m_arcs.push_back( aArc );

    assert( m_shapes.size() == m_points.size() );
}


void SHAPE_LINE_CHAIN::Insert( size_t aVertex, const VECTOR2I& aP )
{
    if( m_shapes[aVertex] != SHAPE_IS_PT )
        convertArc( aVertex );

    m_points.insert( m_points.begin() + aVertex, aP );
    m_shapes.insert( m_shapes.begin() + aVertex, ssize_t( SHAPE_IS_PT ) );

    assert( m_shapes.size() == m_points.size() );
}


void SHAPE_LINE_CHAIN::Insert( size_t aVertex, const SHAPE_ARC& aArc )
{
    if( m_shapes[aVertex] != SHAPE_IS_PT )
        convertArc( aVertex );

    /// Step 1: Find the position for the new arc in the existing arc vector
    size_t arc_pos = m_arcs.size();

    for( auto arc_it = m_shapes.rbegin() ;
              arc_it != m_shapes.rend() + aVertex;
              arc_it++ )
    {
        if( *arc_it != SHAPE_IS_PT )
            arc_pos = ( *arc_it )++;
    }

    m_arcs.insert( m_arcs.begin() + arc_pos, aArc );

    /// Step 2: Add the arc polyline points to the chain
    auto& chain = aArc.ConvertToPolyline();
    m_points.insert( m_points.begin() + aVertex,
            chain.CPoints().begin(), chain.CPoints().end() );

    /// Step 3: Add the vector of indices to the shape vector
    std::vector<size_t> new_points( chain.PointCount(), arc_pos );
    m_shapes.insert( m_shapes.begin() + aVertex, new_points.begin(), new_points.end() );
    assert( m_shapes.size() == m_points.size() );
}


struct compareOriginDistance
{
    compareOriginDistance( VECTOR2I& aOrigin ) :
        m_origin( aOrigin ) {};

    bool operator()( const SHAPE_LINE_CHAIN::INTERSECTION& aA,
                     const SHAPE_LINE_CHAIN::INTERSECTION& aB )
    {
        return ( m_origin - aA.p ).EuclideanNorm() < ( m_origin - aB.p ).EuclideanNorm();
    }

    VECTOR2I m_origin;
};


int SHAPE_LINE_CHAIN::Intersect( const SEG& aSeg, INTERSECTIONS& aIp ) const
{
    for( int s = 0; s < SegmentCount(); s++ )
    {
        OPT_VECTOR2I p = CSegment( s ).Intersect( aSeg );

        if( p )
        {
            INTERSECTION is;
            is.our = CSegment( s );
            is.their = aSeg;
            is.p = *p;
            aIp.push_back( is );
        }
    }

    compareOriginDistance comp( aSeg.A );
    sort( aIp.begin(), aIp.end(), comp );

    return aIp.size();
}

static inline void addIntersection( SHAPE_LINE_CHAIN::INTERSECTIONS& aIps, int aPc, const SHAPE_LINE_CHAIN::INTERSECTION& aP )
{
    if( aIps.size() == 0 )
    {
        aIps.push_back( aP );
        return;
    }

    const auto& last = aIps.back();

    if( ( (last.our.Index() + 1) % aPc) == aP.our.Index() && last.p == aP.p )
        return;

    if( last.our.Index() == aP.our.Index() && last.p == aP.p )
        return;

    aIps.push_back( aP );
}

int SHAPE_LINE_CHAIN::Intersect( const SHAPE_LINE_CHAIN& aChain, INTERSECTIONS& aIp ) const
{
    BOX2I bb_other = aChain.BBox();

    for( int s1 = 0; s1 < SegmentCount(); s1++ )
    {
        const SEG& a = CSegment( s1 );
        const BOX2I bb_cur( a.A, a.B - a.A );

        if( !bb_other.Intersects( bb_cur ) )
            continue;

        for( int s2 = 0; s2 < aChain.SegmentCount(); s2++ )
        {
            const SEG& b = aChain.CSegment( s2 );
            INTERSECTION is;

            if( a.Collinear( b ) )
            {
                is.our = a;
                is.their = b;

                if( a.Contains( b.A ) ) { is.p = b.A; addIntersection(aIp, PointCount(), is); }
                if( a.Contains( b.B ) ) { is.p = b.B; addIntersection(aIp, PointCount(), is); }
                if( b.Contains( a.A ) ) { is.p = a.A; addIntersection(aIp, PointCount(), is); }
                if( b.Contains( a.B ) ) { is.p = a.B; addIntersection(aIp, PointCount(), is); }
            }
            else
            {
                OPT_VECTOR2I p = a.Intersect( b );

                if( p )
                {
                    is.p = *p;
                    is.our = a;
                    is.their = b;
                    addIntersection(aIp, PointCount(), is);
                }
            }
        }
    }

    return aIp.size();
}


int SHAPE_LINE_CHAIN::PathLength( const VECTOR2I& aP ) const
{
    int sum = 0;

    for( int i = 0; i < SegmentCount(); i++ )
    {
        const SEG seg = CSegment( i );
        int d = seg.Distance( aP );

        if( d <= 1 )
        {
            sum += ( aP - seg.A ).EuclideanNorm();
            return sum;
        }
        else
            sum += seg.Length();
    }

    return -1;
}


bool SHAPE_LINE_CHAIN::PointInside( const VECTOR2I& aPt, int aAccuracy, bool aUseBBoxCache ) const
{
    /*
     * Don't check the bounding box unless it's cached.  Building it is about the same speed as
     * the rigorous test below and so just slows things down by doing potentially two tests.
     */
    if( aUseBBoxCache && !m_bbox.Contains( aPt ) )
        return false;

    if( !m_closed || PointCount() < 3 )
        return false;

    bool inside = false;

    /**
     * To check for interior points, we draw a line in the positive x direction from
     * the point.  If it intersects an even number of segments, the point is outside the
     * line chain (it had to first enter and then exit).  Otherwise, it is inside the chain.
     *
     * Note: slope might be denormal here in the case of a horizontal line but we require our
     * y to move from above to below the point (or vice versa)
     *
     * Note: we open-code CPoint() here so that we don't end up calculating the size of the
     * vector number-of-points times.  This has a non-trivial impact on zone fill times.
     */
    const std::vector<VECTOR2I>& points = CPoints();
    int pointCount = points.size();

    for( int i = 0; i < pointCount; )
    {
        const auto p1 = points[ i++ ];
        const auto p2 = points[ i == pointCount ? 0 : i ];
        const auto diff = p2 - p1;

        if( diff.y != 0 )
        {
            const int d = rescale( diff.x, ( aPt.y - p1.y ), diff.y );

            if( ( ( p1.y > aPt.y ) != ( p2.y > aPt.y ) ) && ( aPt.x - p1.x < d ) )
                inside = !inside;
        }
    }

    // If accuracy is 0 then we need to make sure the point isn't actually on the edge.
    // If accuracy is 1 then we don't really care whether or not the point is *exactly* on the
    // edge, so we skip edge processing for performance.
    // If accuracy is > 1, then we use "OnEdge(accuracy-1)" as a proxy for "Inside(accuracy)".
    if( aAccuracy == 0 )
        return inside && !PointOnEdge( aPt );
    else if( aAccuracy == 1 )
        return inside;
    else
        return inside || PointOnEdge( aPt, aAccuracy - 1 );
}


bool SHAPE_LINE_CHAIN::PointOnEdge( const VECTOR2I& aPt, int aAccuracy ) const
{
	return EdgeContainingPoint( aPt, aAccuracy ) >= 0;
}

int SHAPE_LINE_CHAIN::EdgeContainingPoint( const VECTOR2I& aPt, int aAccuracy ) const
{
    if( !PointCount() )
		return -1;

	else if( PointCount() == 1 )
    {
	    VECTOR2I dist = m_points[0] - aPt;
	    return ( hypot( dist.x, dist.y ) <= aAccuracy + 1 ) ? 0 : -1;
    }

    for( int i = 0; i < SegmentCount(); i++ )
    {
        const SEG s = CSegment( i );

        if( s.A == aPt || s.B == aPt )
            return i;

        if( s.Distance( aPt ) <= aAccuracy + 1 )
            return i;
    }

    return -1;
}


bool SHAPE_LINE_CHAIN::CheckClearance( const VECTOR2I& aP, const int aDist) const
{
    if( !PointCount() )
        return false;

    else if( PointCount() == 1 )
        return m_points[0] == aP;

    for( int i = 0; i < SegmentCount(); i++ )
    {
        const SEG s = CSegment( i );

        if( s.A == aP || s.B == aP )
            return true;

        if( s.Distance( aP ) <= aDist )
            return true;
    }

    return false;
}


const OPT<SHAPE_LINE_CHAIN::INTERSECTION> SHAPE_LINE_CHAIN::SelfIntersecting() const
{
    for( int s1 = 0; s1 < SegmentCount(); s1++ )
    {
        for( int s2 = s1 + 1; s2 < SegmentCount(); s2++ )
        {
            const VECTOR2I s2a = CSegment( s2 ).A, s2b = CSegment( s2 ).B;

            if( s1 + 1 != s2 && CSegment( s1 ).Contains( s2a ) )
            {
                INTERSECTION is;
                is.our = CSegment( s1 );
                is.their = CSegment( s2 );
                is.p = s2a;
                return is;
            }
            else if( CSegment( s1 ).Contains( s2b ) &&
                     // for closed polylines, the ending point of the
                     // last segment == starting point of the first segment
                     // this is a normal case, not self intersecting case
                     !( IsClosed() && s1 == 0 && s2 == SegmentCount()-1 ) )
            {
                INTERSECTION is;
                is.our = CSegment( s1 );
                is.their = CSegment( s2 );
                is.p = s2b;
                return is;
            }
            else
            {
                OPT_VECTOR2I p = CSegment( s1 ).Intersect( CSegment( s2 ), true );

                if( p )
                {
                    INTERSECTION is;
                    is.our = CSegment( s1 );
                    is.their = CSegment( s2 );
                    is.p = *p;
                    return is;
                }
            }
        }
    }

    return OPT<SHAPE_LINE_CHAIN::INTERSECTION>();
}


SHAPE_LINE_CHAIN& SHAPE_LINE_CHAIN::Simplify()
{
    std::vector<VECTOR2I> pts_unique;
    std::vector<ssize_t> shapes_unique;

    if( PointCount() < 2 )
    {
        return *this;
    }
    else if( PointCount() == 2 )
    {
        if( m_points[0] == m_points[1] )
            m_points.pop_back();

        return *this;
    }

    int i = 0;
    int np = PointCount();

    // stage 1: eliminate duplicate vertices
    while( i < np )
    {
        int j = i + 1;

        while( j < np && m_points[i] == m_points[j] && m_shapes[i] == m_shapes[j] )
            j++;

        pts_unique.push_back( CPoint( i ) );
        shapes_unique.push_back( m_shapes[i] );

        i = j;
    }

    m_points.clear();
    m_shapes.clear();
    np = pts_unique.size();

    i = 0;

    // stage 1: eliminate collinear segments
    while( i < np - 2 )
    {
        const VECTOR2I p0 = pts_unique[i];
        const VECTOR2I p1 = pts_unique[i + 1];
        int n = i;

        while( n < np - 2
                && ( SEG( p0, p1 ).LineDistance( pts_unique[n + 2] ) <= 1
                        || SEG( p0, p1 ).Collinear( SEG( p1, pts_unique[n + 2] ) ) ) )
            n++;

        m_points.push_back( p0 );
        m_shapes.push_back( shapes_unique[i] );

        if( n > i )
            i = n;

        if( n == np )
        {
            m_points.push_back( pts_unique[n - 1] );
            m_shapes.push_back( shapes_unique[n - 1] );
            return *this;
        }

        i++;
    }

    if( np > 1 )
    {
        m_points.push_back( pts_unique[np - 2] );
        m_shapes.push_back( shapes_unique[np - 2] );
    }

    m_points.push_back( pts_unique[np - 1] );
    m_shapes.push_back( shapes_unique[np - 1] );

    assert( m_points.size() == m_shapes.size() );

    return *this;
}


const VECTOR2I SHAPE_LINE_CHAIN::NearestPoint( const VECTOR2I& aP ) const
{
    int min_d = INT_MAX;
    int nearest = 0;

    for( int i = 0; i < SegmentCount(); i++ )
    {
        int d = CSegment( i ).Distance( aP );

        if( d < min_d )
        {
            min_d = d;
            nearest = i;
        }
    }

    return CSegment( nearest ).NearestPoint( aP );
}


const VECTOR2I SHAPE_LINE_CHAIN::NearestPoint( const SEG& aSeg, int& dist ) const
{
    int nearest = 0;

    dist = INT_MAX;
    for( int i = 0; i < PointCount(); i++ )
    {
        int d = aSeg.LineDistance( CPoint( i ) );

        if( d < dist )
        {
            dist = d;
            nearest = i;
        }
    }

    return CPoint( nearest );
}


int SHAPE_LINE_CHAIN::NearestSegment( const VECTOR2I& aP ) const
{
    int min_d = INT_MAX;
    int nearest = 0;

    for( int i = 0; i < SegmentCount(); i++ )
    {
        int d = CSegment( i ).Distance( aP );

        if( d < min_d )
        {
            min_d = d;
            nearest = i;
        }
    }

    return nearest;
}


const std::string SHAPE_LINE_CHAIN::Format() const
{
    std::stringstream ss;

    ss << m_points.size() << " " << ( m_closed ? 1 : 0 ) << " " << m_arcs.size() << " ";

    for( int i = 0; i < PointCount(); i++ )
        ss << m_points[i].x << " " << m_points[i].y << " " << m_shapes[i];

    for( size_t i = 0; i < m_arcs.size(); i++ )
        ss << m_arcs[i].GetCenter().x << " " << m_arcs[i].GetCenter().y << " "
        << m_arcs[i].GetP0().x << " " << m_arcs[i].GetP0().y << " "
        << m_arcs[i].GetCentralAngle();

    return ss.str();
}


bool SHAPE_LINE_CHAIN::CompareGeometry ( const SHAPE_LINE_CHAIN & aOther ) const
{
    SHAPE_LINE_CHAIN a(*this), b( aOther );
    a.Simplify();
    b.Simplify();

    if( a.m_points.size() != b.m_points.size() )
        return false;

    for( int i = 0; i < a.PointCount(); i++)
        if( a.CPoint( i ) != b.CPoint( i ) )
            return false;
    return true;
}


bool SHAPE_LINE_CHAIN::Intersects( const SHAPE_LINE_CHAIN& aChain ) const
{
    INTERSECTIONS dummy;
    return Intersect( aChain, dummy ) != 0;
}


SHAPE* SHAPE_LINE_CHAIN::Clone() const
{
    return new SHAPE_LINE_CHAIN( *this );
}

bool SHAPE_LINE_CHAIN::Parse( std::stringstream& aStream )
{
    size_t n_pts;
    size_t n_arcs;

    m_points.clear();
    aStream >> n_pts;

    // Rough sanity check, just make sure the loop bounds aren't absolutely outlandish
    if( n_pts > aStream.str().size() )
        return false;

    aStream >> m_closed;
    aStream >> n_arcs;

    if( n_arcs > aStream.str().size() )
        return false;

    for( size_t i = 0; i < n_pts; i++ )
    {
        int x, y;
        ssize_t ind;
        aStream >> x;
        aStream >> y;
        m_points.emplace_back( x, y );

        aStream >> ind;
        m_shapes.push_back( ind );
    }

    for( size_t i = 0; i < n_arcs; i++ )
    {
        VECTOR2I p0, pc;
        double angle;

        aStream >> pc.x;
        aStream >> pc.y;
        aStream >> p0.x;
        aStream >> p0.y;
        aStream >> angle;

        m_arcs.emplace_back( pc, p0, angle );
    }

    return true;
}


const VECTOR2I SHAPE_LINE_CHAIN::PointAlong( int aPathLength ) const
{
    int total = 0;

    if( aPathLength == 0 )
        return CPoint( 0 );

    for( int i = 0; i < SegmentCount(); i++ )
    {
        const SEG& s = CSegment( i );
        int l = s.Length();

        if( total + l >= aPathLength )
        {
            VECTOR2I d( s.B - s.A );
            return s.A + d.Resize( aPathLength - total );
        }

        total += l;
    }

    return CPoint( -1 );
}

double SHAPE_LINE_CHAIN::Area() const
{
    // see https://www.mathopenref.com/coordpolygonarea2.html

    if( !m_closed )
        return 0.0;

    double area = 0.0;
    int size = m_points.size();

    for( int i = 0, j = size - 1; i < size; ++i )
    {
        area += ( (double) m_points[j].x + m_points[i].x ) * ( (double) m_points[j].y - m_points[i].y );
        j = i;
    }

    return -area * 0.5;
}

// fixme: check corner cases against current PointInside implementation
bool SHAPE_LINE_CHAIN::PointInside2( const VECTOR2I& aP ) const
{
    if( !IsClosed() || SegmentCount() < 3 )
        return false;

    // returns 0 if false, +1 if true, -1 if pt ON polygon boundary
    int result  = 0;
    size_t cnt  = PointCount();

    auto ip = CPoint( 0 );

    for( size_t i = 1; i <= cnt; ++i )
    {
        auto ipNext = (i == cnt ? CPoint( 0 ) : CPoint( i ));

        if( ipNext.y == aP.y )
        {
            if( (ipNext.x ==aP.x) || ( ip.y ==aP.y
                                        && ( (ipNext.x >aP.x) == (ip.x <aP.x) ) ) )
                return -1;
	}

        if( (ip.y <aP.y) != (ipNext.y <aP.y) )
        {
            if( ip.x >=aP.x )
            {
                if( ipNext.x >aP.x )
                    result = 1 - result;
                else
                {
                    double d = (double) (ip.x -aP.x) * (ipNext.y -aP.y) -
                               (double) (ipNext.x -aP.x) * (ip.y -aP.y);

                    if( !d )
                        return -1;

                    if( (d > 0) == (ipNext.y > ip.y) )
                        result = 1 - result;
                }
            }
            else
            {
                if( ipNext.x >aP.x )
                {
                    double d = (double) (ip.x -aP.x) * (ipNext.y -aP.y) -
                               (double) (ipNext.x -aP.x) * (ip.y -aP.y);

                    if( !d )
                        return -1;

                    if( (d > 0) == (ipNext.y > ip.y) )
                        result = 1 - result;
                }
            }
        }

        ip = ipNext;
    }

    return result > 0;
}
