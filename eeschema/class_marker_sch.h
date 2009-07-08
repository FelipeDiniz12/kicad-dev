/***************************************************/
/* classes to handle markers used in schematic ... */
/***************************************************/

#ifndef _CLASS_MARKER_SCH_H_
#define _CLASS_MARKER_SCH_H_

#include "sch_item_struct.h"
#include "class_marker_base.h"

/* Marker are mainly used to show an ERC error
*/

enum TypeMarker {      /* Markers type */
    MARK_UNSPEC,
    MARK_ERC,
    MARK_PCB,
    MARK_SIMUL,
    MARK_NMAX        /* Lats value: end of list */
};


/* Names for corresponding types of markers: */
extern const wxChar* NameMarqueurType[];


class MARKER_SCH : public SCH_ITEM , public MARKER_BASE
{
public:
    MARKER_SCH( );
    MARKER_SCH( const wxPoint& aPos, const wxString& aText );
    ~MARKER_SCH();
    virtual wxString GetClass() const
    {
        return wxT( "MARKER_SCH" );
    }


    MARKER_SCH* GenCopy();

    virtual void      Draw( WinEDA_DrawPanel* aPanel, wxDC* aDC,
                            const wxPoint& aOffset, int aDraw_mode,
                            int aColor = -1 );


    /**
     * Function Save
     * writes the data structures for this object out to a FILE in "*.sch"
     * format.
     * @param aFile The FILE to write to.
     * @return bool - true if success writing else false.
     */
    bool              Save( FILE* aFile ) const;

    /** Function GetPenSize
     * @return the size of the "pen" that be used to draw or plot this item
     * for a marker, has no meaning, but it is necessary to satisfy the SCH_ITEM class requirements
     */
    virtual int GetPenSize( ) { return 0; };

    /** Function HitTest
     * @return true if the point aPosRef is within item area
     * @param aPosRef = a wxPoint to test
     */
    bool HitTest( const wxPoint& aPosRef )
    {
        return HitTestMarker( aPosRef );
    }

    /**
     * Function GetBoundingBox
     * returns the orthogonal, bounding box of this object for display purposes.
     * This box should be an enclosing perimeter for visible components of this
     * object, and the units should be in the pcb or schematic coordinate system.
     * It is OK to overestimate the size by a few counts.
     */
    virtual EDA_Rect GetBoundingBox();
    
    /** Function DisplayMarkerInfo()
     * Displays the full info of this marker, in a HTML window
     */
    void DisplayMarkerInfo(WinEDA_SchematicFrame * aFrame);

#if defined(DEBUG)
    void              Show( int nestLevel, std::ostream& os );
#endif
};

#endif /* _CLASS_MARKER_SCH_H_ */
