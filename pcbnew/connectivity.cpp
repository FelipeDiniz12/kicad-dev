/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2017 CERN
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
#define PROFILE

#ifdef PROFILE
#include <profile.h>
#endif

#include <connectivity.h>
#include <connectivity_algo.h>
#include <ratsnest_data.h>

#ifdef USE_OPENMP
#include <omp.h>
#endif /* USE_OPENMP */

CONNECTIVITY_DATA::CONNECTIVITY_DATA()
{
    m_connAlgo.reset( new CN_CONNECTIVITY_ALGO );
}


CONNECTIVITY_DATA::~CONNECTIVITY_DATA()
{
    Clear();
}


bool CONNECTIVITY_DATA::Add( BOARD_ITEM* aItem )
{
    m_connAlgo->Add( aItem );
    return true;
}


bool CONNECTIVITY_DATA::Remove( BOARD_ITEM* aItem )
{
    m_connAlgo->Remove( aItem );
    return true;
}


/**
 * Function Update()
 * Updates the connectivity data for an item.
 * @param aItem is an item to be updated.
 * @return True if operation succeeded. The item will not be updated if it was not previously
 * added to the ratsnest.
 */
bool CONNECTIVITY_DATA::Update( BOARD_ITEM* aItem )
{
    m_connAlgo->Remove( aItem );
    m_connAlgo->Add( aItem );
    return true;
}


void CONNECTIVITY_DATA::Build( BOARD* aBoard )
{
    m_connAlgo.reset( new CN_CONNECTIVITY_ALGO );
    m_connAlgo->Build( aBoard );

    RecalculateRatsnest();
}


void CONNECTIVITY_DATA::Build( const std::vector<BOARD_ITEM*>& aItems )
{
    m_connAlgo.reset( new CN_CONNECTIVITY_ALGO );
    m_connAlgo->Build( aItems );

    RecalculateRatsnest();
}


void CONNECTIVITY_DATA::updateRatsnest()
{
    int lastNet = m_connAlgo->NetCount();

    #ifdef PROFILE
    PROF_COUNTER rnUpdate( "update-ratsnest" );
    #endif

    int nDirty = 0;

    int i;

    #ifdef USE_OPENMP
            #pragma omp parallel shared(lastNet) private(i)
    {
                #pragma omp for schedule(guided, 1)
    #else /* USE_OPENMP */
    {
    #endif

        // Start with net number 1, as 0 stands for not connected
        for( i = 1; i < lastNet; ++i )
        {
            if( m_nets[i]->IsDirty() )
            {
                m_nets[i]->Update();
                nDirty++;
            }
        }
    }          /* end of parallel section */
    #ifdef PROFILE
    rnUpdate.Show();
    #endif /* PROFILE */
    printf( "Dirty: %d\n", nDirty );
}


void CONNECTIVITY_DATA::addRatsnestCluster( std::shared_ptr<CN_CLUSTER> aCluster )
{
    auto rnNet = m_nets[ aCluster->OriginNet() ];

    rnNet->AddCluster( aCluster );
}


void CONNECTIVITY_DATA::RecalculateRatsnest()
{
    int lastNet = m_connAlgo->NetCount();

    if( lastNet >= (int) m_nets.size() )
    {
        unsigned int prevSize = m_nets.size();
        m_nets.resize( lastNet + 1 );

        for( unsigned int i = prevSize; i < m_nets.size(); i++ )
            m_nets[i] = new RN_NET;
    }

    auto clusters = m_connAlgo->GetClusters();

    int dirtyNets = 0;

    for( int net = 0; net < lastNet; net++ )
        if( m_connAlgo->IsNetDirty( net ) )
        {
            m_nets[net]->Clear();
            dirtyNets++;
        }



    for( auto c : clusters )
    {
        int net = c->OriginNet();

        if( m_connAlgo->IsNetDirty( net ) )
        {
            addRatsnestCluster( c );
        }
    }

    m_connAlgo->ClearDirtyFlags();

    updateRatsnest();
}


void CONNECTIVITY_DATA::blockRatsnestItems( const std::vector<BOARD_ITEM*>& aItems )
{
    std::vector<BOARD_CONNECTED_ITEM*> citems;

    for( auto item : aItems )
    {
        if( item->Type() == PCB_MODULE_T )
        {
            for( auto pad : static_cast<MODULE*>(item)->PadsIter() )
                citems.push_back( pad );
        }
        else
        {
            citems.push_back( static_cast<BOARD_CONNECTED_ITEM*>(item) );
        }
    }

    for( auto item : citems )
    {
        auto& entry = m_connAlgo->ItemEntry( item );

        for( auto cnItem : entry.GetItems() )
        {
            for( auto anchor : cnItem->Anchors() )
                anchor->SetNoLine( true );
        }
    }
}


int CONNECTIVITY_DATA::GetNetCount() const
{
    return m_connAlgo->NetCount();
}


void CONNECTIVITY_DATA::FindIsolatedCopperIslands( ZONE_CONTAINER* aZone,
        std::vector<int>& aIslands )
{
    m_connAlgo->FindIsolatedCopperIslands( aZone, aIslands );
}


void CONNECTIVITY_DATA::ComputeDynamicRatsnest( const std::vector<BOARD_ITEM*>& aItems )
{
    m_dynamicConnectivity.reset( new CONNECTIVITY_DATA );
    m_dynamicConnectivity->Build( aItems );

    m_dynamicRatsnest.clear();

    blockRatsnestItems( aItems );

    for( unsigned int nc = 1; nc < m_dynamicConnectivity->m_nets.size(); nc++ )
    {
        auto dynNet = m_dynamicConnectivity->m_nets[nc];

        if( dynNet->GetNodeCount() != 0 )
        {
            auto ourNet = m_nets[nc];
            CN_ANCHOR_PTR nodeA, nodeB;

            if( ourNet->NearestBicoloredPair( *dynNet, nodeA, nodeB ) )
            {
                RN_DYNAMIC_LINE l;
                l.a = nodeA->Pos();
                l.b = nodeB->Pos();
                l.netCode = nc;

                m_dynamicRatsnest.push_back( l );
            }
        }
    }

    for( auto net : m_dynamicConnectivity->m_nets )
    {
        if( !net )
            continue;

        const auto& edges = net->GetUnconnected();

        if( edges.empty() )
            continue;

        for( const auto& edge : edges )
        {
            const auto& nodeA   = edge.GetSourceNode();
            const auto& nodeB   = edge.GetTargetNode();
            RN_DYNAMIC_LINE l;

            l.a = nodeA->Pos();
            l.b = nodeB->Pos();
            l.netCode = 0;
            m_dynamicRatsnest.push_back( l );
        }
    }
}


const std::vector<RN_DYNAMIC_LINE>& CONNECTIVITY_DATA::GetDynamicRatsnest() const
{
    return m_dynamicRatsnest;
}


void CONNECTIVITY_DATA::ClearDynamicRatsnest()
{
    m_dynamicConnectivity.reset();
    m_dynamicRatsnest.clear();
}


void CONNECTIVITY_DATA::PropagateNets()
{
    m_connAlgo->PropagateNets();
}


unsigned int CONNECTIVITY_DATA::GetUnconnectedCount() const
{
    unsigned int unconnected = 0;

    for( auto net : m_nets )
    {
        if( !net )
            continue;

        const auto& edges = net->GetUnconnected();

        if( edges.empty() )
            continue;

        unconnected += edges.size();
    }

    return unconnected;
}


void CONNECTIVITY_DATA::Clear()
{
    for( auto net : m_nets )
        delete net;

    m_nets.clear();
}


const std::list<BOARD_CONNECTED_ITEM*> CONNECTIVITY_DATA::GetConnectedItems(
        const BOARD_CONNECTED_ITEM* aItem,
        const KICAD_T aTypes[] ) const
{
    std::list<BOARD_CONNECTED_ITEM*> rv;
    const auto clusters = m_connAlgo->SearchClusters( CN_CONNECTIVITY_ALGO::CSM_CONNECTIVITY_CHECK, aTypes, aItem->GetNetCode() );

    for ( auto cl : clusters )
        if ( cl->Contains (aItem ) )
        {
            for ( const auto item : *cl )
                rv.push_back( item->Parent() );
        }

    return rv;
}


const std::list<BOARD_CONNECTED_ITEM*> CONNECTIVITY_DATA::GetNetItems(
        int aNetCode,
        const KICAD_T aTypes[] ) const
{

}

bool CONNECTIVITY_DATA::CheckConnectivity( std::vector<CN_DISJOINT_NET_ENTRY>& aReport )
{
    RecalculateRatsnest();

    for ( auto net : m_nets )
    {
        if ( net )
        {
            for ( const auto& edge: net->GetEdges() )
            {
                CN_DISJOINT_NET_ENTRY ent;
                ent.net = edge.GetSourceNode()->Parent()->GetNetCode();
                ent.a = edge.GetSourceNode()->Parent();
                ent.b = edge.GetTargetNode()->Parent();
                ent.anchorA = edge.GetSourceNode()->Pos();
                ent.anchorB = edge.GetTargetNode()->Pos();
                aReport.push_back( ent );
            }
        }
    }
    return aReport.empty();
}
