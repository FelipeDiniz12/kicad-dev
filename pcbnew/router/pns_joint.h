/*
 * KiRouter - a push-and-(sometimes-)shove PCB router
 *
 * Copyright (C) 2013  CERN
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
 * with this program.  If not, see <http://www.gnu.or/licenses/>.
 */

#ifndef __PNS_JOINT_H
#define __PNS_JOINT_H

#include <vector>
#include <boost/functional/hash.hpp>

#include <math/vector2d.h>

#include "pns_item.h"
#include "pns_segment.h"

/**
 * Class PNS_JOINT
 *
 * Represents a 2D point on a given set of layers and belonging to a certain
 * net, that links together a number of board items.
 * A hash table of joints is used by the router to follow connectivity between
 * the items.
 **/
class PNS_JOINT : public PNS_ITEM
{
public:
    typedef std::vector<PNS_ITEM*> LinkedItems;

    ///> Joints are hashed by their position, layers and net.
    ///  Linked items are, obviously, not hashed
    struct HashTag
    {
        VECTOR2I pos;
        int net;
    };

    PNS_JOINT() :
        PNS_ITEM( JOINT ) {}

    PNS_JOINT( const VECTOR2I& aPos, const PNS_LAYERSET& aLayers,
            int aNet = -1 ) :
        PNS_ITEM( JOINT )
    {
        m_tag.pos = aPos;
        m_tag.net = aNet;
        m_layers = aLayers;
    }

    PNS_JOINT( const PNS_JOINT& b ) :
        PNS_ITEM( JOINT )
    {
        m_layers = b.m_layers;
        m_tag.pos = b.m_tag.pos;
        m_tag.net = b.m_tag.net;
        m_linkedItems = b.m_linkedItems;
        m_layers = b.m_layers;
    }

    PNS_ITEM* Clone() const
    {
        assert( false );
        return NULL;
    }

    ///> Returns true if the joint is a trivial line corner, connecting two
    /// segments of the same net, on the same layer.
    bool IsLineCorner() const
    {
        if( m_linkedItems.size() != 2 )
            return false;

        if( m_linkedItems[0]->GetKind() != SEGMENT ||
                m_linkedItems[1]->GetKind() != SEGMENT )
            return false;

        PNS_SEGMENT* seg1 = static_cast<PNS_SEGMENT*>( m_linkedItems[0] );
        PNS_SEGMENT* seg2 = static_cast<PNS_SEGMENT*>( m_linkedItems[1] );

        // joints between segments of different widths are not trivial.
        return seg1->GetWidth() == seg2->GetWidth();
    }

    ///> Links the joint to a given board item (when it's added to the PNS_NODE)
    void Link( PNS_ITEM* aItem )
    {
        LinkedItems::iterator f = std::find( m_linkedItems.begin(),
                                             m_linkedItems.end(), aItem );

        if( f != m_linkedItems.end() )
            return;

        m_linkedItems.push_back( aItem );
    }

    ///> Unlinks a given board item from the joint (upon its removal from a PNS_NODE)
    ///> Returns true if the joint became dangling after unlinking.
    bool Unlink( PNS_ITEM* aItem )
    {
        LinkedItems::iterator f = std::find( m_linkedItems.begin(),
                                             m_linkedItems.end(), aItem );

        if( f != m_linkedItems.end() )
            m_linkedItems.erase( f );

        return m_linkedItems.size() == 0;
    }

    ///> For trivial joints, returns the segment adjacent to (aCurrent). For non-trival ones, returns
    ///> NULL, indicating the end of line.
    PNS_SEGMENT* NextSegment( PNS_SEGMENT* aCurrent ) const
    {
        if( !IsLineCorner() )
            return NULL;

        return static_cast<PNS_SEGMENT*>( m_linkedItems[m_linkedItems[0] == aCurrent ? 1 : 0] );
    }

    /// trivial accessors
    const HashTag& GetTag() const { return m_tag; }
    const VECTOR2I& GetPos() const { return m_tag.pos; }
    int GetNet() const { return m_tag.net; }
    LinkedItems& GetLinkList() { return m_linkedItems; };

    ///> Returns the number of linked items of types listed in aMask.
    int LinkCount( int aMask = -1 ) const
    {
        int n = 0;

        for( LinkedItems::const_iterator i = m_linkedItems.begin();
                                         i != m_linkedItems.end(); ++i )
            if( (*i)->GetKind() & aMask )
                n++;

        return n;
    }

    void Dump() const;

    bool operator==( const PNS_JOINT& rhs )  const
    {
        return m_tag.pos == rhs.m_tag.pos && m_tag.net == rhs.m_tag.net;
    }

    void Merge( const PNS_JOINT& aJoint )
    {
        if( !Overlaps( aJoint ) )
            return;

        m_layers.Merge( aJoint.m_layers );

        // fixme: duplicate links (?)
        for( LinkedItems::const_iterator i = aJoint.m_linkedItems.begin();
             i != aJoint.m_linkedItems.end(); ++i )
            m_linkedItems.push_back( *i );
    }

    bool Overlaps( const PNS_JOINT& rhs ) const
    {
        return m_tag.pos == rhs.m_tag.pos &&
            m_tag.net == rhs.m_tag.net && m_layers.Overlaps( rhs.m_layers );
    }

private:
    ///> hash tag for unordered_multimap
    HashTag m_tag;

    ///> list of items linked to this joint
    LinkedItems m_linkedItems;
};


// hash function & comparison operator for boost::unordered_map<>
inline bool operator==( PNS_JOINT::HashTag const& p1,
                        PNS_JOINT::HashTag const& p2 )
{
    return p1.pos == p2.pos && p1.net == p2.net;
}


inline std::size_t hash_value( PNS_JOINT::HashTag const& p )
{
    std::size_t seed = 0;
    boost::hash_combine( seed, p.pos.x );
    boost::hash_combine( seed, p.pos.y );
    boost::hash_combine( seed, p.net );

    return seed;
}

#endif    // __PNS_JOINT_H
