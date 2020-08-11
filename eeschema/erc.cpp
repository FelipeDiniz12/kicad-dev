/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2011-2016 Wayne Stambaugh <stambaughw@verizon.net>
 * Copyright (C) 1992-2016 KiCad Developers, see AUTHORS.txt for contributors.
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
 * @file erc.cpp
 * @brief Electrical Rules Check implementation.
 */

#include <fctsys.h>
#include <kicad_string.h>
#include <sch_edit_frame.h>
#include <netlist_object.h>
#include <lib_pin.h>
#include <erc.h>
#include <sch_marker.h>
#include <sch_sheet.h>
#include <sch_reference_list.h>
#include <schematic.h>
#include <wx/ffile.h>
#include <ws_draw_item.h>
#include <ws_proxy_view_item.h>


/* ERC tests :
 *  1 - conflicts between connected pins ( example: 2 connected outputs )
 *  2 - minimal connections requirements ( 1 input *must* be connected to an
 * output, or a passive pin )
 */


/*
 *  Minimal ERC requirements:
 *  All pins *must* be connected (except ELECTRICAL_PINTYPE::PT_NC).
 *  When a pin is not connected in schematic, the user must place a "non
 * connected" symbol to this pin.
 *  This ensures a forgotten connection will be detected.
 */

/* Messages for conflicts :
 *  ELECTRICAL_PINTYPE::PT_INPUT, ELECTRICAL_PINTYPE::PT_OUTPUT, ELECTRICAL_PINTYPE:PT_:BIDI, ELECTRICAL_PINTYPE::PT_TRISTATE, ELECTRICAL_PINTYPE::PT_PASSIVE,
 *  ELECTRICAL_PINTYPE::PT_UNSPECIFIED, ELECTRICAL_PINTYPE::PT_POWER_IN, ELECTRICAL_PINTYPE::PT_POWER_OUT, ELECTRICAL_PINTYPE::PT_OPENCOLLECTOR,
 *  ELECTRICAL_PINTYPE::PT_OPENEMITTER, ELECTRICAL_PINTYPE::PT_NC
 *  These messages are used to show the ERC matrix in ERC dialog
 */

// Messages for matrix rows:
const wxString CommentERC_H[] =
{
    _( "Input Pin" ),
    _( "Output Pin" ),
    _( "Bidirectional Pin" ),
    _( "Tri-State Pin" ),
    _( "Passive Pin" ),
    _( "Unspecified Pin" ),
    _( "Power Input Pin" ),
    _( "Power Output Pin" ),
    _( "Open Collector" ),
    _( "Open Emitter" ),
    _( "No Connection" )
};

// Messages for matrix columns
const wxString CommentERC_V[] =
{
    _( "Input Pin" ),
    _( "Output Pin" ),
    _( "Bidirectional Pin" ),
    _( "Tri-State Pin" ),
    _( "Passive Pin" ),
    _( "Unspecified Pin" ),
    _( "Power Input Pin" ),
    _( "Power Output Pin" ),
    _( "Open Collector" ),
    _( "Open Emitter" ),
    _( "No Connection" )
};


int ERC_TESTER::TestDuplicateSheetNames( bool aCreateMarker )
{
    SCH_SCREEN* screen;
    int         err_count = 0;

    SCH_SCREENS screenList( m_schematic->Root() );

    for( screen = screenList.GetFirst(); screen != nullptr; screen = screenList.GetNext() )
    {
        std::vector<SCH_SHEET*> list;

        for( SCH_ITEM* item : screen->Items().OfType( SCH_SHEET_T ) )
            list.push_back( static_cast<SCH_SHEET*>( item ) );

        for( size_t i = 0; i < list.size(); i++ )
        {
            SCH_SHEET* sheet = list[i];

            for( size_t j = i + 1; j < list.size(); j++ )
            {
                SCH_SHEET* test_item = list[j];

                // We have found a second sheet: compare names
                // we are using case insensitive comparison to avoid mistakes between
                // similar names like Mysheet and mysheet
                if( sheet->GetName().CmpNoCase( test_item->GetName() ) == 0 )
                {
                    if( aCreateMarker )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_DUPLICATE_SHEET_NAME );
                        ercItem->SetItems( sheet, test_item );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, sheet->GetPosition() );
                        screen->Append( marker );
                    }

                    err_count++;
                }
            }
        }
    }

    return err_count;
}


void ERC_TESTER::TestTextVars( KIGFX::WS_PROXY_VIEW_ITEM* aWorksheet )
{
    WS_DRAW_ITEM_LIST wsItems;

    auto unresolved = [this]( wxString str )
    {
        str = ExpandEnvVarSubstitutions( str, &m_schematic->Prj() );
        return str.Matches( wxT( "*${*}*" ) );
    };

    if( aWorksheet )
    {
        wsItems.SetMilsToIUfactor( IU_PER_MILS );
        wsItems.BuildWorkSheetGraphicList( aWorksheet->GetPageInfo(), aWorksheet->GetTitleBlock() );
    }

    SCH_SCREENS screens( m_schematic->Root() );

    for( SCH_SCREEN* screen = screens.GetFirst(); screen != NULL; screen = screens.GetNext() )
    {
        for( SCH_ITEM* item : screen->Items().OfType( SCH_LOCATE_ANY_T ) )
        {
            if( item->Type() == SCH_COMPONENT_T )
            {
                SCH_COMPONENT* component = static_cast<SCH_COMPONENT*>( item );

                for( SCH_FIELD& field : component->GetFields() )
                {
                    if( unresolved( field.GetShownText() ) )
                    {
                        wxPoint pos = field.GetPosition() - component->GetPosition();
                        pos = component->GetTransform().TransformCoordinate( pos );
                        pos += component->GetPosition();

                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( &field );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, pos );
                        screen->Append( marker );
                    }
                }
            }
            else if( item->Type() == SCH_SHEET_T )
            {
                SCH_SHEET* sheet = static_cast<SCH_SHEET*>( item );

                for( SCH_FIELD& field : sheet->GetFields() )
                {
                    if( unresolved( field.GetShownText() ) )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( &field );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, field.GetPosition() );
                        screen->Append( marker );
                    }
                }

                for( SCH_SHEET_PIN* pin : static_cast<SCH_SHEET*>( item )->GetPins() )
                {
                    if( pin->GetShownText().Matches( wxT( "*${*}*" ) ) )
                    {
                        std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                        ercItem->SetItems( pin );

                        SCH_MARKER* marker = new SCH_MARKER( ercItem, pin->GetPosition() );
                        screen->Append( marker );
                    }
                }
            }
            else if( SCH_TEXT* text = dynamic_cast<SCH_TEXT*>( item ) )
            {
                if( text->GetShownText().Matches( wxT( "*${*}*" ) ) )
                {
                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                    ercItem->SetItems( text );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, text->GetPosition() );
                    screen->Append( marker );
                }
            }
        }

        for( WS_DRAW_ITEM_BASE* item = wsItems.GetFirst(); item; item = wsItems.GetNext() )
        {
            if( WS_DRAW_ITEM_TEXT* text = dynamic_cast<WS_DRAW_ITEM_TEXT*>( item ) )
            {
                if( text->GetShownText().Matches( wxT( "*${*}*" ) ) )
                {
                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_UNRESOLVED_VARIABLE );
                    ercItem->SetErrorMessage( _( "Unresolved text variable in worksheet." ) );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, text->GetPosition() );
                    screen->Append( marker );
                }
            }
        }
    }
}


int ERC_TESTER::TestConflictingBusAliases()
{
    wxString    msg;
    int         err_count = 0;

    SCH_SCREENS screens( m_schematic->Root() );
    std::vector< std::shared_ptr<BUS_ALIAS> > aliases;

    for( SCH_SCREEN* screen = screens.GetFirst(); screen != NULL; screen = screens.GetNext() )
    {
        std::unordered_set< std::shared_ptr<BUS_ALIAS> > screen_aliases = screen->GetBusAliases();

        for( const std::shared_ptr<BUS_ALIAS>& alias : screen_aliases )
        {
            for( const std::shared_ptr<BUS_ALIAS>& test : aliases )
            {
                if( alias->GetName() == test->GetName() && alias->Members() != test->Members() )
                {
                    msg.Printf( _( "Bus alias %s has conflicting definitions on %s and %s" ),
                                alias->GetName(),
                                alias->GetParent()->GetFileName(),
                                test->GetParent()->GetFileName() );

                    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_BUS_ALIAS_CONFLICT );
                    ercItem->SetErrorMessage( msg );

                    SCH_MARKER* marker = new SCH_MARKER( ercItem, wxPoint() );
                    test->GetParent()->Append( marker );

                    ++err_count;
                }
            }
        }

        aliases.insert( aliases.end(), screen_aliases.begin(), screen_aliases.end() );
    }

    return err_count;
}


int ERC_TESTER::TestMultiunitFootprints()
{
    SCH_SHEET_LIST sheets = m_schematic->GetSheets();

    int errors = 0;
    std::map<wxString, LIB_ID> footprints;
    SCH_MULTI_UNIT_REFERENCE_MAP refMap;
    sheets.GetMultiUnitComponents( refMap, true );

    for( auto& component : refMap )
    {
        auto& refList = component.second;

        if( refList.GetCount() == 0 )
        {
            wxFAIL;   // it should not happen
            continue;
        }

        // Reference footprint
        SCH_COMPONENT* unit = nullptr;
        wxString       unitName;
        wxString       unitFP;

        for( unsigned i = 0; i < component.second.GetCount(); ++i )
        {
            SCH_SHEET_PATH sheetPath = refList.GetItem( i ).GetSheetPath();
            unitFP = refList.GetItem( i ).GetComp()->GetField( FOOTPRINT )->GetText();

            if( !unitFP.IsEmpty() )
            {
                unit = refList.GetItem( i ).GetComp();
                unitName = unit->GetRef( &sheetPath, true );
                break;
            }
        }

        for( unsigned i = 0; i < component.second.GetCount(); ++i )
        {
            SCH_REFERENCE& secondRef = refList.GetItem( i );
            SCH_COMPONENT* secondUnit = secondRef.GetComp();
            wxString       secondName = secondUnit->GetRef( &secondRef.GetSheetPath(), true );
            const wxString secondFp = secondUnit->GetField( FOOTPRINT )->GetText();
            wxString       msg;

            if( !secondFp.IsEmpty() && unitFP != secondFp )
            {
                msg.Printf( _( "Different footprints assigned to %s and %s" ),
                            unitName, secondName );

                std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_DIFFERENT_UNIT_FP );
                ercItem->SetErrorMessage( msg );
                ercItem->SetItems( unit, secondUnit );

                SCH_MARKER* marker = new SCH_MARKER( ercItem, secondUnit->GetPosition() );
                secondRef.GetSheetPath().LastScreen()->Append( marker );

                ++errors;
            }
        }
    }

    return errors;
}


void ERC_TESTER::diagnose( NETLIST_OBJECT* aNetItemRef, NETLIST_OBJECT* aNetItemTst, int aMinConn,
                           PIN_ERROR aDiag )
{
    if( aDiag == PIN_ERROR::OK || aMinConn < 1 || aNetItemRef->m_Type != NETLIST_ITEM::PIN )
        return;

    ERC_SETTINGS& settings = m_schematic->ErcSettings();

    SCH_PIN* pin = static_cast<SCH_PIN*>( aNetItemRef->m_Comp );

    if( aNetItemTst == NULL)
    {
        if( aMinConn == NOD )    /* Nothing driving the net. */
        {
            if( settings.GetSeverity( ERCE_PIN_NOT_DRIVEN ) != RPT_SEVERITY_IGNORE )
            {
                std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_PIN_NOT_DRIVEN );
                ercItem->SetItems( pin );

                SCH_MARKER* marker = new SCH_MARKER( ercItem, aNetItemRef->m_Start );
                aNetItemRef->m_SheetPath.LastScreen()->Append( marker );
            }
            return;
        }
    }

    if( aNetItemTst && aNetItemTst->m_Type == NETLIST_ITEM::PIN )  /* Error between 2 pins */
    {
        if( settings.GetSeverity( ERCE_PIN_TO_PIN_WARNING ) != RPT_SEVERITY_IGNORE )
        {
            std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create(
                    aDiag == PIN_ERROR::PP_ERROR ? ERCE_PIN_TO_PIN_ERROR : ERCE_PIN_TO_PIN_WARNING );
            ercItem->SetItems( pin, static_cast<SCH_PIN*>( aNetItemTst->m_Comp ) );

            SCH_MARKER* marker = new SCH_MARKER( ercItem, aNetItemRef->m_Start );
            aNetItemRef->m_SheetPath.LastScreen()->Append( marker );
        }
    }
}


void ERC_TESTER::TestOthersItems( NETLIST_OBJECT_LIST* aList, unsigned aNetItemRef,
                                  unsigned aNetStart, int* aMinConnexion )
{
    ERC_SETTINGS& settings = m_schematic->ErcSettings();

    unsigned netItemTst = aNetStart;
    ELECTRICAL_PINTYPE jj;
    PIN_ERROR erc = PIN_ERROR::OK;

    /* Analysis of the table of connections. */
    ELECTRICAL_PINTYPE ref_elect_type = aList->GetItem( aNetItemRef )->m_ElectricalPinType;
    int local_minconn = NOC;

    if( ref_elect_type == ELECTRICAL_PINTYPE::PT_NC )
        local_minconn = NPI;

    /* Test pins connected to NetItemRef */
    for( ; ; netItemTst++ )
    {
        if( aNetItemRef == netItemTst )
            continue;

        // We examine only a given net. We stop the search if the net changes
        if( ( netItemTst >= aList->size() ) // End of list
            || ( aList->GetItemNet( aNetItemRef ) !=
                 aList->GetItemNet( netItemTst ) ) ) // End of net
        {
            /* End net code found: minimum connection test. */
            if( ( *aMinConnexion < NET_NC ) && ( local_minconn < NET_NC ) )
            {
                /* Not connected or not driven pin. */
                bool seterr = true;

                if( local_minconn == NOC && aList->GetItemType( aNetItemRef ) == NETLIST_ITEM::PIN )
                {
                    /* This pin is not connected: for multiple part per
                     * package, and duplicated pin,
                     * search for another instance of this pin
                     * this will be flagged only if all instances of this pin
                     * are not connected
                     * TODO test also if instances connected are connected to
                     * the same net
                     */
                    for( unsigned duplicate = 0; duplicate < aList->size(); duplicate++ )
                    {
                        if( aList->GetItemType( duplicate ) != NETLIST_ITEM::PIN )
                            continue;

                        if( duplicate == aNetItemRef )
                            continue;

                        if( aList->GetItem( aNetItemRef )->m_PinNum !=
                            aList->GetItem( duplicate )->m_PinNum )
                            continue;

                        if( ( (SCH_COMPONENT*) aList->GetItem( aNetItemRef )->
                             m_Link )->GetRef( &aList->GetItem( aNetItemRef )-> m_SheetPath ) !=
                            ( (SCH_COMPONENT*) aList->GetItem( duplicate )->m_Link )
                           ->GetRef( &aList->GetItem( duplicate )->m_SheetPath ) )
                            continue;

                        // Same component and same pin. Do dot create error for this pin
                        // if the other pin is connected (i.e. if duplicate net has another
                        // item)
                        if( (duplicate > 0)
                          && ( aList->GetItemNet( duplicate ) ==
                               aList->GetItemNet( duplicate - 1 ) ) )
                            seterr = false;

                        if( (duplicate < aList->size() - 1)
                          && ( aList->GetItemNet( duplicate ) ==
                               aList->GetItemNet( duplicate + 1 ) ) )
                            seterr = false;
                    }
                }

                if( seterr )
                {
                    diagnose( aList->GetItem( aNetItemRef ), nullptr, local_minconn,
                            PIN_ERROR::WARNING );
                }

                *aMinConnexion = DRV;   // inhibiting other messages of this
                                       // type for the net.
            }
            return;
        }

        switch( aList->GetItemType( netItemTst ) )
        {
        case NETLIST_ITEM::ITEM_UNSPECIFIED:
        case NETLIST_ITEM::SEGMENT:
        case NETLIST_ITEM::BUS:
        case NETLIST_ITEM::JUNCTION:
        case NETLIST_ITEM::LABEL:
        case NETLIST_ITEM::HIERLABEL:
        case NETLIST_ITEM::BUSLABELMEMBER:
        case NETLIST_ITEM::HIERBUSLABELMEMBER:
        case NETLIST_ITEM::SHEETBUSLABELMEMBER:
        case NETLIST_ITEM::SHEETLABEL:
        case NETLIST_ITEM::GLOBLABEL:
        case NETLIST_ITEM::GLOBBUSLABELMEMBER:
        case NETLIST_ITEM::PINLABEL:
            break;

        case NETLIST_ITEM::NOCONNECT:
            local_minconn = std::max( NET_NC, local_minconn );
            break;

        case NETLIST_ITEM::PIN:
            jj            = aList->GetItem( netItemTst )->m_ElectricalPinType;
            local_minconn = std::max( settings.GetPinMinDrive( ref_elect_type, jj ),
                                      local_minconn );

            if( netItemTst <= aNetItemRef )
                break;

            if( erc == PIN_ERROR::OK )
            {
                erc = settings.GetPinMapValue( ref_elect_type, jj );

                if( erc != PIN_ERROR::OK )
                {
                    if( aList->GetConnectionType( netItemTst ) == NET_CONNECTION::UNCONNECTED )
                    {
                        aList->SetConnectionType( netItemTst,
                                                  NET_CONNECTION::NOCONNECT_SYMBOL_PRESENT );
                    }

                    diagnose( aList->GetItem( aNetItemRef ), aList->GetItem( netItemTst ), 1, erc );
                }
            }

            break;
        }
    }
}


int ERC_TESTER::TestNoConnectPins()
{
    int err_count = 0;

    for( const SCH_SHEET_PATH& sheet : m_schematic->GetSheets() )
    {
        std::map<wxPoint, std::vector<SCH_PIN*>> pinMap;

        for( SCH_ITEM* item : sheet.LastScreen()->Items().OfType( SCH_COMPONENT_T ) )
        {
            SCH_COMPONENT* comp = static_cast<SCH_COMPONENT*>( item );

            for( SCH_PIN* pin : comp->GetSchPins( &sheet ) )
            {
                if( pin->GetLibPin()->GetType() == ELECTRICAL_PINTYPE::PT_NC )
                    pinMap[pin->GetPosition()].emplace_back( pin );
            }
        }

        for( auto& pair : pinMap )
        {
            if( pair.second.size() > 1 )
            {
                err_count++;

                std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_NOCONNECT_CONNECTED );

                ercItem->SetItems( pair.second[0], pair.second[1],
                                   pair.second.size() > 2 ? pair.second[2] : nullptr,
                                   pair.second.size() > 3 ? pair.second[3] : nullptr );
                ercItem->SetErrorMessage( _( "Pins with \"no connection\" type are connected" ) );

                SCH_MARKER* marker = new SCH_MARKER( ercItem, pair.first );
                sheet.LastScreen()->Append( marker );
            }
        }
    }

    return err_count;
}


// this code try to detect similar labels, i.e. labels which are identical
// when they are compared using case insensitive coparisons.


// A helper struct to compare NETLIST_OBJECT items by sheetpath and label texts
// for a std::set<NETLIST_OBJECT*> container
// the full text is "sheetpath+label" for local labels and "label" for global labels
struct compare_labels
{
    bool operator() ( const NETLIST_OBJECT* lab1, const NETLIST_OBJECT* lab2 ) const
    {
        wxString str1 = lab1->m_SheetPath.PathAsString() + lab1->m_Label;
        wxString str2 = lab2->m_SheetPath.PathAsString() + lab2->m_Label;

        return str1.Cmp( str2 ) < 0;
    }
};

struct compare_label_names
{
    bool operator() ( const NETLIST_OBJECT* lab1, const NETLIST_OBJECT* lab2 ) const
    {
        return lab1->m_Label.Cmp( lab2->m_Label ) < 0;
    }
};

struct compare_paths
{
    bool operator() ( const NETLIST_OBJECT* lab1, const NETLIST_OBJECT* lab2 ) const
    {
        return lab1->m_SheetPath.Path() < lab2->m_SheetPath.Path();
    }
};

// Helper functions to build the warning messages about Similar Labels:
static int countIndenticalLabels( std::vector<NETLIST_OBJECT*>& aList, NETLIST_OBJECT* aRef );
static void SimilarLabelsDiagnose( NETLIST_OBJECT* aItemA, NETLIST_OBJECT* aItemB );


void NETLIST_OBJECT_LIST::TestforSimilarLabels()
{
    // Similar labels which are different when using case sensitive comparisons
    // but are equal when using case insensitive comparisons

    // list of all labels (used the better item to build diag messages)
    std::vector<NETLIST_OBJECT*> fullLabelList;
    // list of all labels , each label appears only once (used to to detect similar labels)
    std::set<NETLIST_OBJECT*, compare_labels> uniqueLabelList;
    wxString msg;

    // Build a list of differents labels. If inside a given sheet there are
    // more than one given label, only one label is stored.
    // not also the sheet labels are not taken in account for 2 reasons:
    //  * they are in the root sheet but they are seen only from the child sheet
    //  * any mismatch between child sheet hierarchical labels and the sheet label
    //    already detected by ERC
    for( unsigned netItem = 0; netItem < size(); ++netItem )
    {
        switch( GetItemType( netItem ) )
        {
        case NETLIST_ITEM::LABEL:
        case NETLIST_ITEM::BUSLABELMEMBER:
        case NETLIST_ITEM::PINLABEL:
        case NETLIST_ITEM::GLOBBUSLABELMEMBER:
        case NETLIST_ITEM::HIERLABEL:
        case NETLIST_ITEM::HIERBUSLABELMEMBER:
        case NETLIST_ITEM::GLOBLABEL:
            // add this label in lists
            uniqueLabelList.insert( GetItem( netItem ) );
            fullLabelList.push_back( GetItem( netItem ) );
            break;

        case NETLIST_ITEM::SHEETLABEL:
        case NETLIST_ITEM::SHEETBUSLABELMEMBER:
        default:
            break;
        }
    }

    // build global labels and compare
    std::set<NETLIST_OBJECT*, compare_label_names> loc_labelList;

    for( NETLIST_OBJECT* label : uniqueLabelList )
    {
        if( label->IsLabelGlobal() )
            loc_labelList.insert( label );
    }

    // compare global labels (same label names appears only once in list)
    for( auto it = loc_labelList.begin(); it != loc_labelList.end(); ++it )
    {
        auto it_aux = it;

        for( ++it_aux; it_aux != loc_labelList.end(); ++it_aux )
        {
            if( (*it)->m_Label.CmpNoCase( (*it_aux)->m_Label ) == 0 )
            {
                // Create new marker for ERC.
                int cntA = countIndenticalLabels( fullLabelList, *it );
                int cntB = countIndenticalLabels( fullLabelList, *it_aux );

                if( cntA <= cntB )
                    SimilarLabelsDiagnose( (*it), (*it_aux) );
                else
                    SimilarLabelsDiagnose( (*it_aux), (*it) );
            }
        }
    }

    // Build paths list
    std::set<NETLIST_OBJECT*, compare_paths> pathsList;

    for( NETLIST_OBJECT* label : uniqueLabelList )
        pathsList.insert( label );

    // Examine each label inside a sheet path:
    for( NETLIST_OBJECT* candidate : pathsList )
    {
        loc_labelList.clear();

        for( NETLIST_OBJECT* uniqueLabel : uniqueLabelList)
        {
            if( candidate->m_SheetPath.Path() == uniqueLabel->m_SheetPath.Path() )
                loc_labelList.insert( uniqueLabel );
        }

        // at this point, loc_labelList contains labels of the current sheet path.
        // Detect similar labels (same label names appears only once in list)

        for( auto ref_it = loc_labelList.begin(); ref_it != loc_labelList.end(); ++ref_it )
        {
            NETLIST_OBJECT* ref_item = *ref_it;
            auto it_aux = ref_it;

            for( ++it_aux; it_aux != loc_labelList.end(); ++it_aux )
            {
                // global label versus global label was already examined.
                // here, at least one label must be local
                if( ref_item->IsLabelGlobal() && ( *it_aux )->IsLabelGlobal() )
                    continue;

                if( ref_item->m_Label.CmpNoCase( ( *it_aux )->m_Label ) == 0 )
                {
                    // Create new marker for ERC.
                    int cntA = countIndenticalLabels( fullLabelList, ref_item );
                    int cntB = countIndenticalLabels( fullLabelList, *it_aux );

                    if( cntA <= cntB )
                        SimilarLabelsDiagnose( ref_item, ( *it_aux ) );
                    else
                        SimilarLabelsDiagnose( ( *it_aux ), ref_item );
                }
            }
        }
    }
}


// Helper function: count the number of labels identical to aLabel
//  for global label: global labels in the full project
//  for local label: all labels in the current sheet
static int countIndenticalLabels( std::vector<NETLIST_OBJECT*>& aList, NETLIST_OBJECT* aRef )
{
    int count = 0;

    if( aRef->IsLabelGlobal() )
    {
        for( NETLIST_OBJECT* i : aList )
        {
            if( i->IsLabelGlobal() && i->m_Label == aRef->m_Label )
                count++;
        }
    }
    else
    {
        for( NETLIST_OBJECT* i : aList )
        {
            if( i->m_Label == aRef->m_Label && i->m_SheetPath.Path() == aRef->m_SheetPath.Path() )
                count++;
        }
    }

    return count;
}


// Helper function: creates a marker for similar labels ERC warning
static void SimilarLabelsDiagnose( NETLIST_OBJECT* aItemA, NETLIST_OBJECT* aItemB )
{
    std::shared_ptr<ERC_ITEM> ercItem = ERC_ITEM::Create( ERCE_SIMILAR_LABELS );
    ercItem->SetItems( aItemA->m_Comp, aItemB->m_Comp );

    SCH_MARKER* marker = new SCH_MARKER( ercItem, aItemA->m_Start );
    aItemA->m_SheetPath.LastScreen()->Append( marker );
}
