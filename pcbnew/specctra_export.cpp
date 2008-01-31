/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2007-2008 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2007 Kicad Developers, see change_log.txt for contributors.
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

 
/*  This source is a complement to specctra.cpp and implements the export to
    specctra dsn file format.  The specification for the grammar of the specctra
    dsn file used to develop this code is given here:
    http://www.autotraxeda.com/docs/SPECCTRA/SPECCTRA.pdf
        
    Also see the comments at the top of the specctra.cpp file itself.
*/


#include "specctra.h"
#include "collectors.h"
#include "wxPcbStruct.h"        // Change_Side_Module()
#include "pcbstruct.h"          // HISTORY_NUMBER

using namespace DSN;


// see wxPcbStruct.h
void WinEDA_PcbFrame::ExportToSPECCTRA( wxCommandEvent& event )
{
    wxString        fullFileName = GetScreen()->m_FileName;
    wxString        std_ext = wxT( ".dsn" );
    wxString        mask    = wxT( "*" ) + std_ext;
    
    ChangeFileNameExt( fullFileName, std_ext );
    
    fullFileName = EDA_FileSelector( _( "Specctra DSN file:" ),
                                     wxEmptyString,     /* Chemin par defaut */
                                     fullFileName,      /* nom fichier par defaut */
                                     std_ext,           /* extension par defaut */
                                     mask,              /* Masque d'affichage */
                                     this,
                                     wxFD_SAVE,
                                     FALSE
                                     );
    if( fullFileName == wxEmptyString )
        return;

    
    SPECCTRA_DB     db;
    bool            ok = true;
    wxString        errorText;
    
    db.SetPCB( SPECCTRA_DB::MakePCB() );

    try 
    {    
        db.FromBOARD( m_Pcb );
        db.ExportPCB(  fullFileName, true );
    
        // if an exception is thrown by FromBOARD or ExportPCB(), then 
        // ~SPECCTRA_DB() will close the file.
    } 
    catch( IOError ioe )
    {
        ok = false;
        
        // copy the error string to safe place, ioe is in this scope only.
        errorText = ioe.errorText;
    }

    if( ok )
    {
        // @todo display a message saying the export is complete.
    }
    else
        DisplayError( this, errorText );
}


namespace DSN {

struct POINT_PAIR
{
    POINT       p1;         ///< start
    POINT       p2;         ///< end
    BOARD_ITEM* item;       ///< the item which has these points, TRACK or DRAWSEGMENT
};
typedef std::vector<POINT_PAIR>     POINT_PAIRS; 


static inline void swap( POINT_PAIR& pair )
{
    POINT temp = pair.p1;
    pair.p1 = pair.p2;
    pair.p2 = temp;
}


/**
 * Function scale
 * converts a distance from kicad units to our reported specctra dsn units:
 * 1/10000 inches (deci-mils) to mils.  So the factor of 10 comes in.
 */
static inline double scale( int kicadDist )
{
    return kicadDist/10.0;
}

static inline double mapX( int x )
{
    return scale(x);
}

static inline double mapY( int y )
{
    return -scale(y);      // make y negative, since it is increasing going down.
}


/**
 * Function mapPt
 * converts a Kicad point into a DSN file point.  Kicad's BOARD coordinates
 * are in deci-mils  (i.e. 1/10,000th of an inch) and we are exporting in units
 * of mils, so we have to divide by 10.
 */
static POINT mapPt( const wxPoint& pt )
{
    POINT ret;
    ret.x = mapX( pt.x );
    ret.y = mapY( pt.y );
    ret.FixNegativeZero();
    return ret;
}


/**
 * Function swapEnds
 * will swap ends of any POINT_PAIR in the POINT_PAIRS list in order to
 * make the consecutive POINT_PAIRs be "connected" at their ends.
 */
static void swapEnds( POINT_PAIRS& aList )
{
    POINT   temp;
    
    if( aList.size() <= 1 )
        return;
    
    for( unsigned i=0;  i<aList.size();  ++i )
    {
        if( aList[i].p1 == aList[i+1].p1 )
            swap( aList[i] ); 

        else if( aList[i].p1 == aList[i+1].p2 )
        {
            swap( aList[i] );
            swap( aList[i+1] );
            ++i;    // skip next one, we swapped i+1 here
        }
    }
}


/**
 * Function isRectangle
 * tests to see if the POINT_PAIRS list makes up a vertically/horizontally 
 * oriented rectangle.
 * @return bool - true if there are 4 point pairs making a rectangle.
 */ 
static bool isRectangle( POINT_PAIRS& aList )
{
    if( aList.size() == 4 )
    {
        for( unsigned i=0;  i<aList.size();  ++i )
        {
            if( i < aList.size()-1 )
                if( aList[i].p2 != aList[i+1].p1 )
                    return false;
            
            if( aList[i].p1.x != aList[i].p2.x 
            &&  aList[i].p1.y != aList[i].p2.y )
                return false;
        }
        
        return ( aList[0].p1 == aList[3].p2 );
    }
    return false;
}


/**************************************************************************/
static int Pad_list_Sort_by_Shapes( const void* refptr, const void* objptr )
/**************************************************************************/
{
    const D_PAD* padref = *(D_PAD**)refptr;
    const D_PAD* padcmp = *(D_PAD**)objptr;

    return D_PAD::Compare( padref, padcmp );     
}



/*********************************************************************/
static int Track_list_Sort_by_Netcode( const void* o1, const void* o2 )
/*********************************************************************/
{
    TRACK*  t1 = *(TRACK**) o1;
    TRACK*  t2 = *(TRACK**) o2;
    int     diff;
    
    if( (diff = t1->GetNet() - t2->GetNet()) )
        return diff;
    if( (diff = t1->m_Width - t2->m_Width) )
        return diff;
    if( (diff = t1->GetLayer() - t2->GetLayer()) )
        return diff;

    return diff;    // zero here
}


/**
 * Function makePath
 * creates a PATH element with a single straight line, a pair of vertices.
 */
static PATH* makePath( const POINT& aStart, const POINT& aEnd, const std::string& aLayerName )
{
    PATH*   path = new PATH( 0, T_path );

    path->AppendPoint( aStart );
    path->AppendPoint( aEnd );
    path->SetLayerId( aLayerName.c_str() );
    return path;
}


static QARC* makeArc( const POINT& aStart, const POINT& aEnd, 
                     const POINT& aCenter, const std::string& aLayerName ) 
{
    QARC*   qarc = new QARC(0);
    
    qarc->SetStart( aStart );
    qarc->SetEnd( aEnd );
    qarc->SetCenter( aCenter );
    qarc->SetLayerId( aLayerName.c_str() );
    return qarc;
}


IMAGE* SPECCTRA_DB::makeIMAGE( MODULE* aModule )
{
    PADSTACKS&  padstacks = pcb->library->padstacks;

    TYPE_COLLECTOR  pads;
    static const KICAD_T scanPADs[] = { TYPEPAD, EOT };
    
    // get all the MODULE's pads.        
    pads.Collect( aModule, scanPADs );

    IMAGE*  image = new IMAGE(0);
    
    image->image_id = CONV_TO_UTF8( aModule->m_LibRef );
        
    // from the pads, and make an IMAGE using collated padstacks.
    for( int p=0;  p<pads.GetCount();  ++p )
    {
        D_PAD* pad = (D_PAD*) pads[p];

        // see if this pad is a through hole with no copper on its perimeter
        if( !pad->IsOnLayer( LAYER_CMP_N ) && !pad->IsOnLayer( COPPER_LAYER_N ) )
        {
            if( pad->m_Drill.x != 0 )
            {
                KEEPOUT* keepout = new KEEPOUT(image, T_keepout);
                image->keepouts.push_back( keepout );
                
                CIRCLE* circle = new CIRCLE(keepout);
                keepout->SetShape( circle );
                
                circle->SetDiameter( scale(pad->m_Drill.x) );
                circle->SetVertex( POINT( mapPt( pad->m_Pos0 ) ) );
                circle->layer_id = "signal";
                
                // ?? the keepout is not affecting the power layers? 
            }
        }
        else
        {
            PADSTACK*   padstack = &padstacks[pad->m_logical_connexion];
            
            PIN*    pin = new PIN(image);
            image->pins.push_back( pin );
            
            pin->padstack_id = padstack->padstack_id;
            pin->pin_id      = CONV_TO_UTF8( pad->ReturnStringPadName() );
            pin->SetVertex( mapPt( pad->m_Pos0 ) );
        }
    }
    
    return image;
}    


void SPECCTRA_DB::makePADSTACKs( BOARD* aBoard, TYPE_COLLECTOR& aPads )
{
    char    name[80];       // padstack name builder
    
    if( aPads.GetCount() )
    {
        qsort( (void*) aPads.BasePtr(), aPads.GetCount(), sizeof(D_PAD*), Pad_list_Sort_by_Shapes );
    }

    D_PAD*  old_pad = NULL;

#define COPPER_LAYERS    2      // top and bottom

    for( int i=0;  i<aPads.GetCount();  ++i )
    {
        D_PAD*  pad = (D_PAD*) aPads[i];

        bool doLayer[COPPER_LAYERS] =  { 
            pad->IsOnLayer( LAYER_CMP_N ),
            pad->IsOnLayer( COPPER_LAYER_N ) 
        };
        
        if( old_pad && 0==D_PAD::Compare( old_pad, pad ) )
        {
            // padstacks.size()-1 is the index of the matching padstack in LIBRARY::padstacks
            pad->m_logical_connexion = pcb->library->padstacks.size()-1;
            
            // this is the same as the last pad, so do not add it to the padstack list.
            continue;
        }

        // if pad has no copper presence, then it will be made into
        // an "image->keepout" later.  No copper pad here, it is probably a hole.        
        if( !doLayer[0] && !doLayer[1] )
        {
            // padstacks.size()-1 is the index of the matching padstack in LIBRARY::padstacks
            pad->m_logical_connexion = pcb->library->padstacks.size()-1;
            
            continue;
        }

        old_pad = pad;

        PADSTACK*   padstack = new PADSTACK( pcb->library );
        pcb->library->AddPadstack( padstack );
        
        // padstacks.size()-1 is the index of the matching padstack in LIBRARY::padstacks
        pad->m_logical_connexion = pcb->library->padstacks.size()-1;
        
        // paddOfset is the offset of copper shape relative to hole position, 
        // and pad->m_Pos is hole position. All shapes must be shifted by
        // this distance, normally (0,0).
        
        // Note that the y correction here is set negative.
        POINT       padOffset( scale(pad->m_Offset.x), -scale(pad->m_Offset.y) );

        
        // for now, we will report only one layer for the pads.  SMD pads are reported on the
        // top layer, and through hole are reported on <reserved_layer_name> "signal"        
        int         reportedLayers = 1;     // how many layers are reported.
        
        doLayer[0] = true;
        
        const char* layerName = ( pad->m_Attribut == PAD_SMD ) ? 
                                    layerIds[0].c_str() : "signal"; 

        int         coppers = 0;        // will always be one for now
        
        switch( pad->m_PadShape )
        {
        default:
        case PAD_CIRCLE:
            {
                double  diameter = scale(pad->m_Size.x);
                
                for( int layer=0;  layer<reportedLayers;  ++layer )
                {
                    if( doLayer[layer] )
                    {
                        SHAPE*      shape = new SHAPE( padstack );
                        padstack->Append( shape );
    
                        CIRCLE*     circle = new CIRCLE( shape );
                        shape->SetShape( circle );
                        
                        circle->SetLayerId( layerName );
                        circle->SetDiameter( diameter );
                        circle->SetVertex( padOffset );
                        ++coppers;
                    }
                }
                
                snprintf( name, sizeof(name), "Round%dPad_%.6g_mil", coppers, scale(pad->m_Size.x) );
                name[ sizeof(name)-1 ] = 0;

                // @todo verify that all pad names are unique, there is a chance that 
                // D_PAD::Compare() could say two pads are different, yet the get the same
                // name here. If so, blend in the padNdx into the name.
                
                padstack->SetPadstackId( name );
            }
            break;

        case PAD_RECT:
            {
                double dx = scale( pad->m_Size.x ) / 2.0;
                double dy = scale( pad->m_Size.y ) / 2.0;
                
                POINT   lowerLeft( -dx, -dy );
                POINT   upperRight( dx, dy );

                lowerLeft  += padOffset;
                upperRight += padOffset;

                for( int layer=0;  layer<reportedLayers;  ++layer )
                {
                    if( doLayer[layer] )
                    {
                        SHAPE*      shape = new SHAPE( padstack );
                        padstack->Append( shape );
                        
                        RECTANGLE*  rect = new RECTANGLE( shape );
                        shape->SetShape( rect );
                        
                        rect->SetLayerId( layerName );
                        rect->SetCorners( lowerLeft, upperRight );
                        ++coppers;
                    }
                }
                
                snprintf( name, sizeof(name),  "Rect%dPad_%.6gx%.6g_mil", 
                         coppers, scale(pad->m_Size.x), scale(pad->m_Size.y)  );
                name[ sizeof(name)-1 ] = 0;

                // @todo verify that all pad names are unique, there is a chance that 
                // D_PAD::Compare() could say two pads are different, yet they get the same
                // name here. If so, blend in the padNdx into the name.
                
                padstack->SetPadstackId( name );
            }
            break;
            
        case PAD_OVAL:
            {
                double dx = scale( pad->m_Size.x ) / 2.0;
                double dy = scale( pad->m_Size.y ) / 2.0;
                double dr = dx - dy;

                if( dr >= 0 )       // oval is horizontal
                {
                    double  radius = dy;

                    for( int layer=0;  layer<reportedLayers;  ++layer )
                    {
                        if( doLayer[layer] )
                        {
                            // each oval is 2 lines and 4 (quarter circle) qarcs
        
                            SHAPE*  shape;
                            PATH*   path;
                            QARC*   qarc;
                            
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            path = makePath( 
                                    POINT( -dr + padOffset.x, padOffset.y - radius ),     // aStart
                                    POINT(  dr + padOffset.x, padOffset.y - radius ),     // aEnd
                                    layerName );
                            shape->SetShape( path );
                            
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            // @todo: this 1/2 circle arc needs to be split into two quarter circle arcs 
                            qarc = makeArc( 
                                    POINT(  dr + padOffset.x, padOffset.y - radius),      // aStart
                                    POINT(  dr + padOffset.x, padOffset.y + radius),      // aEnd
                                    POINT(  dr + padOffset.x, padOffset.y ),              // aCenter
                                    layerName );
                            shape->SetShape( qarc );
    
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            path = makePath( 
                                    POINT(  dr + padOffset.x, padOffset.y + radius ),     // aStart
                                    POINT( -dr + padOffset.x, padOffset.y + radius ),     // aEnd
                                    layerName );
                            shape->SetShape( path );
    
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            // @todo: this 1/2 circle arc needs to be split into two quarter circle arcs 
                            qarc = makeArc( 
                                    POINT( -dr + padOffset.x, padOffset.y + radius),      // aStart
                                    POINT( -dr + padOffset.x, padOffset.y - radius),      // aEnd
                                    POINT( -dr + padOffset.x, padOffset.y ),              // aCenter
                                    layerName );
                            shape->SetShape( qarc );
                            ++coppers;
                        }
                    }
                }
                else        // oval is vertical
                {
                    double  radius = dx;
                    
                    dr = -dr;

                    for( int layer=0;  layer<reportedLayers;  ++layer )
                    {
                        if( doLayer[layer] )
                        {
                            // each oval is 2 lines and 2 qarcs
        
                            SHAPE*  shape;
                            PATH*   path;
                            QARC*   qarc;
                            
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            path = makePath( 
                                    POINT( -radius + padOffset.x, padOffset.y - dr ),   // aStart
                                    POINT( -radius + padOffset.x, padOffset.y + dr ),   // aEnd
                                    layerName );
                            shape->SetShape( path );
    
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            // @todo: this 1/2 circle arc needs to be split into two quarter circle arcs 
                            qarc = makeArc( 
                                    POINT( -radius + padOffset.x, padOffset.y + dr ),   // aStart
                                    POINT(  radius + padOffset.x, padOffset.y + dr),    // aEnd
                                    POINT(  padOffset.x, padOffset.y +dr ),             // aCenter
                                    layerName );
                            shape->SetShape( qarc );
    
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            path = makePath( 
                                    POINT(  radius + padOffset.x, padOffset.y + dr ),   // aStart
                                    POINT(  radius + padOffset.x, padOffset.y - dr ),   // aEnd
                                    layerName );
                            shape->SetShape( path );
    
                            shape = new SHAPE( padstack );
                            padstack->Append( shape );
                            // @todo: this 1/2 circle arc needs to be split into two quarter circle arcs 
                            qarc = makeArc( 
                                    POINT(  radius + padOffset.x, padOffset.y - dr),    // aStart
                                    POINT( -radius + padOffset.x, padOffset.y - dr),    // aEnd
                                    POINT( padOffset.x, padOffset.y - dr ),             // aCenter
                                    layerName );
                            shape->SetShape( qarc );
                            ++coppers;
                        }
                    }
                }
                
                snprintf( name, sizeof(name),  "Oval%dPad_%.6gx%.6g_mil", 
                         coppers, scale(pad->m_Size.x), scale(pad->m_Size.y)  );
                name[ sizeof(name)-1 ] = 0;
                
                // @todo verify that all pad names are unique, there is a chance that 
                // D_PAD::Compare() could say two pads are different, yet they get the same
                // name here. If so, blend in the padNdx into the name.
                
                padstack->SetPadstackId( name );
            }
            break;

/*            
        case PAD_TRAPEZOID:
            break;
*/            
        }
    }

    // unique pads are now in the padstack list.  
    // next we add the via's which may be used.

    int defaultViaSize = aBoard->m_BoardSettings->m_CurrentViaSize;
    if( defaultViaSize )
    {
        PADSTACK*   padstack = new PADSTACK( pcb->library );
        pcb->library->AddPadstack( padstack );

        // remember this index, it is the default via and also the start of the 
        // vias within the padstack list.  Before this index are the pads.
        // At this index and later are the vias.
        pcb->library->SetViaStartIndex( pcb->library->padstacks.size()-1 );
        
        SHAPE*      shape = new SHAPE( padstack );
        padstack->Append( shape );

        CIRCLE*     circle = new CIRCLE( shape );
        shape->SetShape( circle );
        circle->SetLayerId( "signal" ); 
        circle->SetDiameter( scale(defaultViaSize) );

        padstack->SetPadstackId( "Via_Default" );
    }

    for( int i=0;  i<HISTORY_NUMBER;  ++i )
    {
        int viaSize = aBoard->m_BoardSettings->m_ViaSizeHistory[i]; 
        if( !viaSize )
            break;
        
        if( viaSize == defaultViaSize )
            continue;
        
        PADSTACK*   padstack = new PADSTACK( pcb->library );
        pcb->library->AddPadstack( padstack );
        
        SHAPE*      shape = new SHAPE( padstack );
        padstack->Append( shape );

        CIRCLE*     circle = new CIRCLE( shape );
        shape->SetShape( circle );
        circle->SetLayerId( "signal" ); 
        circle->SetDiameter( scale(viaSize) );

        snprintf( name, sizeof(name),  "Via_%.6g_mil", scale(viaSize) ); 
        name[ sizeof(name)-1 ] = 0;
        
        padstack->SetPadstackId( name );
    }
}


void SPECCTRA_DB::FromBOARD( BOARD* aBoard )
{
    TYPE_COLLECTOR          items;
    POINT_PAIRS             ppairs;
    POINT_PAIR              pair;

    if( !pcb )
        pcb = SPECCTRA_DB::MakePCB();

    //  DSN Images (=Kicad MODULES and pads) must be presented from the
    //  top view.  So we temporarily flip any modules which are on the back
    //  side of the board to the front, and record this in the MODULE's flag field.
    for( MODULE* module = aBoard->m_Modules;  module;  module = module->Next() )
    {
        module->flag = 0;
        if( module->GetLayer() == COPPER_LAYER_N )
        {
            aBoard->Change_Side_Module( module, NULL );
            module->flag = 1;
        }
    }

    
    // Since none of these statements cause any immediate output, the order
    // of them is somewhat flexible.  The outputting to disk is done at the
    // end.  We start by gathering all the layer information from the board.
    

    //-----<layer_descriptor>-----------------------------------------------
    {
        // specctra wants top physical layer first, then going down to the 
        // bottom most physical layer in physical sequence.
        // @question : why does Kicad not display layers in that order?
        int layerCount = aBoard->GetCopperLayerCount();

        layerIds.clear();
        
        for( int ndx=layerCount-1;  ndx >= 0;  --ndx )
        {
            // save the specctra layer name in SPECCTRA_DB::layerIds for later.
            layerIds.push_back( CONV_TO_UTF8( 
                 aBoard->GetLayerName( ndx>0 && ndx==layerCount-1 ? LAYER_CMP_N : ndx )));

            LAYER*      layer = new LAYER( pcb->structure );
            pcb->structure->layers.push_back( layer );
            
            layer->name = layerIds.back();
            
            // layer->type =  @todo need this, the export would be better.
        }
    }

    
    // for now, report on only the top and bottom layers with respect to the copper
    // within a pad's padstack.  this is usually correct, but not rigorous.  We could do
    // better if there was actually a "layer type" field within Kicad which would
    // hold one of:  T_signal, T_power, T_mixed, T_jumper
    // See bottom of page 74 of the SECCTRA Design Language Reference, May 2000.

    // a space in a quoted token is NOT a terminator, true establishes this.
    pcb->parser->space_in_quoted_tokens = true;
    
    //-----<unit_descriptor> & <resolution_descriptor>--------------------
    {    
        pcb->unit->units = T_mil;
        pcb->resolution->units = T_mil;
        pcb->resolution->value = 100;
    }

    
    //-----<boundary_descriptor>------------------------------------------
    {
        // get all the DRAWSEGMENTS into 'items', then look for layer == EDGE_N,
        // and those segments comprise the board's perimeter.
        static const KICAD_T  scanDRAWSEGMENTS[] = { TYPEDRAWSEGMENT, EOT };
        items.Collect( aBoard, scanDRAWSEGMENTS );
    
        bool haveEdges = false;
        ppairs.clear();
        for( int i=0;  i<items.GetCount();  ++i )
        {
            DRAWSEGMENT* item = (DRAWSEGMENT*) items[i];
            
            wxASSERT( item->Type() == TYPEDRAWSEGMENT );
            
            if( item->GetLayer() == EDGE_N )
            {
                pair.p1 = mapPt( item->m_Start );
                pair.p2 = mapPt( item->m_End );
                pair.item = item;
                ppairs.push_back( pair );
                haveEdges = true;
            }
        }
    
        if( haveEdges )
        {
            swapEnds( ppairs );
    
#if 0 && defined(DEBUG)        
            for( unsigned i=0;  i<ppairs.size();  ++i )
            {
                POINT_PAIR* p = &ppairs[i];
                p->item->Show( 0, std::cout );
            }
#endif        
    
            BOUNDARY*   boundary = new BOUNDARY(0);
    
            if( isRectangle( ppairs ) )
            {
                RECTANGLE*  rect = new RECTANGLE( boundary );
                rect->layer_id = "pcb";
                
                // opposite corners
                rect->SetCorners( ppairs[0].p1, ppairs[2].p1 );
                
                boundary->rectangle = rect;
            }
            else
            {
                PATH*  path = new PATH( boundary );
                
                path->layer_id = "pcb";
                for( unsigned i=0; i<ppairs.size();  ++i )
                {
                    // unless its a closed polygon, this probably won't work,
                    // otherwise it will.
                    path->points.push_back( ppairs[i].p1 );
                }
                
                boundary->paths.push_back( path );
            }
    
            pcb->structure->SetBOUNDARY( boundary );
        }
        else
        {
            aBoard->ComputeBoundaryBox();
            
            BOUNDARY*   boundary = new BOUNDARY(0);
            RECTANGLE*  rect = new RECTANGLE( boundary );
    
            rect->layer_id = "pcb";
            
            // opposite corners
            wxPoint bottomRight;
            bottomRight.x = aBoard->m_BoundaryBox.GetRight();
            bottomRight.y = aBoard->m_BoundaryBox.GetBottom();
            
            rect->SetCorners( mapPt( aBoard->m_BoundaryBox.GetOrigin() ),
                              mapPt( bottomRight ) );
            
            boundary->rectangle = rect;
            
            pcb->structure->SetBOUNDARY( boundary );
        }
    }

    
    //-----<zone containers become planes>--------------------------------------------
    {
        static const KICAD_T  scanZONEs[] = { TYPEZONE_CONTAINER, EOT };
        items.Collect( aBoard, scanZONEs );

        for( int i=0;  i<items.GetCount();  ++i )
        {
            ZONE_CONTAINER* item = (ZONE_CONTAINER*) items[i];

            COPPER_PLANE*   plane = new COPPER_PLANE( pcb->structure );
            PATH*           polygon = new PATH( plane, T_polygon );
            plane->SetShape( polygon );
            
            plane->name = CONV_TO_UTF8( item->m_Netname );

            wxString        layerName = aBoard->GetLayerName( item->GetLayer() );
            polygon->layer_id = CONV_TO_UTF8( layerName );
            
            int count = item->m_Poly->corner.size();
            for( int j=0; j<count; ++j )
            {
                wxPoint   point( item->m_Poly->corner[j].x, item->m_Poly->corner[j].y );

                polygon->points.push_back( mapPt(point) );
            }
            
            pcb->structure->planes.push_back( plane );
        }
    }

    // keepouts could go here, there are none in Kicad at this time.
    // although COPPER_PLANEs probably will need them for the thru holes, etc.
    // but in that case they are WINDOWs within the COPPER_PLANEs.

    
    //-----<build the initial padstack list>--------------------------------
    {
        TYPE_COLLECTOR  pads;
        static const KICAD_T scanPADs[] = { TYPEPAD, EOT };
    
        // get all the D_PADs into 'pads'.        
        pads.Collect( aBoard, scanPADs );

        makePADSTACKs( aBoard, pads );
        
#if 0 && defined(DEBUG)
        for( int p=0;  p<pads.GetCount();  ++p )
            pads[p]->Show( 0, std::cout );
#endif    
    }

    
    //-----<build the images and components>---------------------------------
    {
        static const KICAD_T scanMODULEs[] = { TYPEMODULE, EOT };
        items.Collect( aBoard, scanMODULEs );
        
        for( int m=0;  m<items.GetCount();  ++m )
        {
            MODULE* module = (MODULE*) items[m];
            
            IMAGE*  image  = makeIMAGE( module );

            IMAGE*  registered = pcb->library->LookupIMAGE( image );  
            if( registered != image )
            {
                // If our new 'image' is not a unique IMAGE, delete it.
                // In either case, 'registered' is the one we'll work with henceforth.
                delete image;
            }

            const std::string& imageId = registered->image_id;
            
            COMPONENT* comp = pcb->placement->LookupCOMPONENT( imageId );
            
            PLACE* place = new PLACE( comp );
            comp->places.push_back( place );
            
            place->SetRotation( module->m_Orient/10.0 );
            place->SetVertex( mapPt( module->m_Pos ) );
            place->component_id = CONV_TO_UTF8( module->GetReference() );
            
            /* not supported by freerouting.com yet.
            place->part_number  = CONV_TO_UTF8( module->GetValue() );
            */
            
            // module is flipped from bottom side, set side to T_back
            if( module->flag )      
                place->side = T_back;
        }
    }
    
    
    //-----<via_descriptor>-------------------------------------------------
    {
        // Output the vias in the padstack list here, by name
        VIA*        vias = pcb->structure->via;
        PADSTACKS&  padstacks = pcb->library->padstacks;
        int         viaNdx = pcb->library->via_start_index;

        if( viaNdx != -1 )
        {
            for(  ; viaNdx < (int)padstacks.size();  ++viaNdx )
            {
                vias->AppendVia( padstacks[viaNdx].padstack_id.c_str() );
            }
        }
    }

    
    //-----<create the nets>------------------------------------------------
    {
        NETWORK*    network = pcb->network;
        static const KICAD_T scanNETs[] = { PCB_EQUIPOT_STRUCT_TYPE, EOT };

        items.Collect( aBoard, scanNETs );
        
        PIN_REF emptypin(0);
        
        for( int i=0;  i<items.GetCount();  ++i )
        {
            EQUIPOT*   kinet = (EQUIPOT*) items[i];
            
            if( kinet->GetNet() == 0 )
                continue;

            NET* net = new NET( network );
            network->nets.push_back( net );
            
            net->net_id = CONV_TO_UTF8( kinet->m_Netname );
            net->net_number = kinet->GetNet();
            
            D_PAD**  ppad = kinet->m_PadzoneStart;
            for(   ; ppad < kinet->m_PadzoneEnd;   ++ppad )
            {
                D_PAD* pad = *ppad;

                wxASSERT( pad->Type() == TYPEPAD );
                
                // push on an empty one, then fill it via 'pin_ref'
                net->pins.push_back( emptypin );
                PIN_REF* pin_ref = &net->pins.back();
                
                pin_ref->SetParent( net );
                pin_ref->component_id = CONV_TO_UTF8( ((MODULE*)pad->m_Parent)->GetReference() );;
                pin_ref->pin_id = CONV_TO_UTF8( pad->ReturnStringPadName() );               
            }
        }
    }


    //-----<create the wires from tracks>-----------------------------------
    {
#if 0        
        // export all of them for now, later we'll decide what controls we need
        // on this.
        static KICAD_T scanTRACKs[] = { TYPETRACK, TYPEVIA, EOT };
        
        items.Collect( aBoard, scanTRACKs );

        if( items.GetCount() )
            qsort( (void*) items.BasePtr(), items.GetCount(), 
                  sizeof(TRACK*), Track_list_Sort_by_Netcode );

        WIRING* wiring = pcb->wiring;
        WIRE*   wire;

        int     old_netcode = -1; 
        int     old_width = -1; 
        int     old_layer = -1;        

        for( int i=0;  i<items.GetCount();  ++i )
        {
            TRACK*  track = (TRACK*) items[i];  // torv == track or via
            
            if( track->GetNet() == 0 )
                continue;
            
            if( old_netcode != track->GetNet() )
            {
                old_netcode = track->GetNet();
                
                EQUIPOT* equipot = aBoard->FindNet( track->GetNet() );
                wxASSERT( equipot );
                
                wire = new WIRE( wiring );
                wiring->wires.push_back( wire );
            
                wire->net_id = CONV_TO_UTF8( equipot->m_Netname );
            }

            if( old_width != track->m_Width )
            {
                old_width = track->m_Width;
                wire = new WIRE( wiring );
                wiring.wires.push_back( wire );

                wire->net_id = CONV_TO_UTF8( equipot->m_NetName );
            }
    
            if( (track->Type() == TYPETRACK) || (track->Type() == TYPEZONE) )
            {
                if( old_layer != track->GetLayer() )
                {
                    old_layer = track->GetLayer();
                    fprintf( file, "LAYER %s\n",
                            CONV_TO_UTF8( GenCAD_Layer_Name[track->GetLayer() & 0x1F] ) );
                }
    
                fprintf( file, "LINE %d %d %d %d\n",
                        mapXto( track->m_Start.x ), mapYto( track->m_Start.y ),
                        mapXto( track->m_End.x ), mapYto( track->m_End.y ) );
            }
            if( track->Type() == TYPEVIA )
            {
                fprintf( file, "VIA viapad%d %d %d ALL %d via%d\n",
                         track->m_Width,
                         mapXto( track->m_Start.x ), mapYto( track->m_Start.y ),
                         g_DesignSettings.m_ViaDrill, vianum++ );
            }
        }
#endif        
    }
    
    //-----<restore MODULEs>------------------------------------------------
    
    //  DSN Images (=Kicad MODULES and pads) must be presented from the
    //  top view.  Restore those that were flipped.
    for( MODULE* module = aBoard->m_Modules;  module;  module = module->Next() )
    {
        if( module->flag )
        {
            aBoard->Change_Side_Module( module, NULL );
            module->flag = 0;
        }
    }
}

    
}       // namespace DSN

