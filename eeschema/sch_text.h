/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef CLASS_TEXT_LABEL_H
#define CLASS_TEXT_LABEL_H


#include <macros.h>
#include <eda_text.h>
#include <sch_item.h>
#include <sch_connection.h>   // for CONNECTION_TYPE


class NETLIST_OBJECT_LIST;

/*
 * Spin style for text items of all kinds on schematics
 * Basically a higher level abstraction of rotation and justification of text
 */
class LABEL_SPIN_STYLE
{
public:
    enum SPIN : int
    {
        LEFT   = 0,
        UP     = 1,
        RIGHT  = 2,
        BOTTOM = 3
    };


    LABEL_SPIN_STYLE() = default;
    constexpr LABEL_SPIN_STYLE( SPIN aSpin ) : m_spin( aSpin )
    {
    }

    constexpr bool operator==( SPIN a ) const
    {
        return m_spin == a;
    }

    constexpr bool operator!=( SPIN a ) const
    {
        return m_spin != a;
    }

    operator int() const
    {
        return static_cast<int>( m_spin );
    }

    LABEL_SPIN_STYLE RotateCW()
    {
        SPIN newSpin = m_spin;

        switch( m_spin )
        {
        case LABEL_SPIN_STYLE::LEFT:   newSpin = LABEL_SPIN_STYLE::UP;     break;
        case LABEL_SPIN_STYLE::UP:     newSpin = LABEL_SPIN_STYLE::RIGHT;  break;
        case LABEL_SPIN_STYLE::RIGHT:  newSpin = LABEL_SPIN_STYLE::BOTTOM; break;
        case LABEL_SPIN_STYLE::BOTTOM: newSpin = LABEL_SPIN_STYLE::LEFT;   break;
        default: wxLogWarning( "RotateCCW encountered unknown current spin style" ); break;
        }

        return LABEL_SPIN_STYLE( newSpin );
    }

    LABEL_SPIN_STYLE RotateCCW()
    {
        SPIN newSpin = m_spin;

        switch( m_spin )
        {
        case LABEL_SPIN_STYLE::LEFT:   newSpin = LABEL_SPIN_STYLE::BOTTOM; break;
        case LABEL_SPIN_STYLE::BOTTOM: newSpin = LABEL_SPIN_STYLE::RIGHT;  break;
        case LABEL_SPIN_STYLE::RIGHT:  newSpin = LABEL_SPIN_STYLE::UP;     break;
        case LABEL_SPIN_STYLE::UP:     newSpin = LABEL_SPIN_STYLE::LEFT;   break;
        default: wxLogWarning( "RotateCCW encountered unknown current spin style" ); break;
        }

        return LABEL_SPIN_STYLE( newSpin );
    }

    /*
     * Mirrors the label spin style across the X axis or simply swaps up and bottom
     */
    LABEL_SPIN_STYLE MirrorX()
    {
        SPIN newSpin = m_spin;

        switch( m_spin )
        {
        case LABEL_SPIN_STYLE::UP:     newSpin = LABEL_SPIN_STYLE::BOTTOM; break;
        case LABEL_SPIN_STYLE::BOTTOM: newSpin = LABEL_SPIN_STYLE::UP;     break;
        case LABEL_SPIN_STYLE::LEFT:                                       break;
        case LABEL_SPIN_STYLE::RIGHT:                                      break;
        default: wxLogWarning( "MirrorX encountered unknown current spin style" ); break;
        }

        return LABEL_SPIN_STYLE( newSpin );
    }

    /*
     * Mirrors the label spin style across the Y axis or simply swaps left and right
     */
    LABEL_SPIN_STYLE MirrorY()
    {
        SPIN newSpin = m_spin;

        switch( m_spin )
        {
        case LABEL_SPIN_STYLE::LEFT:  newSpin = LABEL_SPIN_STYLE::RIGHT; break;
        case LABEL_SPIN_STYLE::RIGHT: newSpin = LABEL_SPIN_STYLE::LEFT;  break;
        case LABEL_SPIN_STYLE::UP:                                       break;
        case LABEL_SPIN_STYLE::BOTTOM:                                   break;
        default: wxLogWarning( "MirrorY encountered unknown current spin style" ); break;
        }

        return LABEL_SPIN_STYLE( newSpin );
    }

private:
    SPIN m_spin;
};

/* Shape/Type of SCH_HIERLABEL and SCH_GLOBALLABEL
 * mainly used to handle the graphic associated shape
 */
enum class PINSHEETLABEL_SHAPE
{
    PS_INPUT,           // use "PS_INPUT" instead of "INPUT" to avoid colliding
                        // with a Windows header on msys2
    PS_OUTPUT,
    PS_BIDI,
    PS_TRISTATE,
    PS_UNSPECIFIED
};


extern const char* SheetLabelType[];    /* names of types of labels */


class SCH_TEXT : public SCH_ITEM, public EDA_TEXT
{
protected:
    PINSHEETLABEL_SHAPE m_shape;

    /// True if not connected to another object if the object derive from SCH_TEXT
    /// supports connections.
    bool m_isDangling;

    CONNECTION_TYPE m_connectionType;

    /**
     * The orientation of text and any associated drawing elements of derived objects.
     * 0 is the horizontal and left justified.
     * 1 is vertical and top justified.
     * 2 is horizontal and right justified.  It is the equivalent of the mirrored 0 orentation.
     * 3 is veritcal and bottom justifiend. It is the equivalent of the mirrored 1 orentation.
     * This is a duplicattion of m_Orient, m_HJustified, and m_VJustified in #EDA_TEXT but is
     * easier to handle than 3 parameters when editing and reading and saving files.
     */
    LABEL_SPIN_STYLE m_spin_style;

public:
    SCH_TEXT( const wxPoint& aPos = wxPoint( 0, 0 ), const wxString& aText = wxEmptyString,
              KICAD_T aType = SCH_TEXT_T );

    /**
     * Clones \a aText into a new object.  All members are copied as is except
     * for the #m_isDangling member which is set to false.  This prevents newly
     * copied objects derived from #SCH_TEXT from having their connection state
     * improperly set.
     */
    SCH_TEXT( const SCH_TEXT& aText );

    ~SCH_TEXT() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_TEXT_T == aItem->Type();
    }

    virtual wxString GetClass() const override
    {
        return wxT( "SCH_TEXT" );
    }

    /**
     * Returns the set of contextual text variable tokens for this text item.
     * @param aVars [out]
     */
    void GetContextualTextVars( wxArrayString* aVars ) const;

    wxString GetShownText( int aDepth = 0 ) const override;

    /**
     * Increment the label text, if it ends with a number.
     *
     * @param aIncrement = the increment value to add to the number ending the text.
     */
    void IncrementLabel( int aIncrement );

    /**
     * Set a spin or rotation angle, along with specific horizontal and vertical justification
     * styles with each angle.
     *
     * @param aSpinStyle Spin style as per LABEL_SPIN_STYLE storage class, may be the enum values or int value
     */
    virtual void     SetLabelSpinStyle( LABEL_SPIN_STYLE aSpinStyle );
    LABEL_SPIN_STYLE GetLabelSpinStyle() const
    {
        return m_spin_style;
    }

    PINSHEETLABEL_SHAPE GetShape() const        { return m_shape; }

    void SetShape( PINSHEETLABEL_SHAPE aShape ) { m_shape = aShape; }

    /**
     * @return the offset between the SCH_TEXT position and the text itself position
     *
     * This offset depends on the orientation, the type of text, and the area required to
     * draw the associated graphic symbol or to put the text above a wire.
     */
    virtual wxPoint GetSchematicTextOffset( RENDER_SETTINGS* aSettings ) const;

    void Print( RENDER_SETTINGS* aSettings, const wxPoint& offset ) override;

    /**
     * Calculate the graphic shape (a polygon) associated to the text.
     *
     * @param aPoints A buffer to fill with polygon corners coordinates
     * @param Pos Position of the shape, for texts and labels: do nothing
     * Mainly for derived classes (SCH_SHEET_PIN and Hierarchical labels)
     */
    virtual void CreateGraphicShape( RENDER_SETTINGS* aSettings,
                                     std::vector <wxPoint>& aPoints, const wxPoint& Pos )
    {
        aPoints.clear();
    }

    void SwapData( SCH_ITEM* aItem ) override;

    const EDA_RECT GetBoundingBox() const override;

    bool operator<( const SCH_ITEM& aItem ) const override;

    int GetTextOffset( RENDER_SETTINGS* aSettings ) const;

    int GetPenWidth() const override;

    // Geometric transforms (used in block operations):

    void Move( const wxPoint& aMoveVector ) override
    {
        EDA_TEXT::Offset( aMoveVector );
    }

    void MirrorY( int aYaxis_position ) override;
    void MirrorX( int aXaxis_position ) override;
    void Rotate( wxPoint aPosition ) override;

    bool Matches( wxFindReplaceData& aSearchData, void* aAuxData ) override
    {
        return SCH_ITEM::Matches( GetText(), aSearchData );
    }

    bool Replace( wxFindReplaceData& aSearchData, void* aAuxData ) override
    {
        return EDA_TEXT::Replace( aSearchData );
    }

    virtual bool IsReplaceable() const override { return true; }

    void GetEndPoints( std::vector< DANGLING_END_ITEM >& aItemList ) override;

    bool UpdateDanglingState( std::vector<DANGLING_END_ITEM>& aItemList,
                              const SCH_SHEET_PATH* aPath = nullptr ) override;

    bool IsDangling() const override { return m_isDangling; }
    void SetIsDangling( bool aIsDangling ) { m_isDangling = aIsDangling; }

    void GetConnectionPoints( std::vector< wxPoint >& aPoints ) const override;

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    void GetNetListItem( NETLIST_OBJECT_LIST& aNetListItems, SCH_SHEET_PATH* aSheetPath ) override;

    wxPoint GetPosition() const override { return EDA_TEXT::GetTextPos(); }
    void SetPosition( const wxPoint& aPosition ) override { EDA_TEXT::SetTextPos( aPosition ); }

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    void Plot( PLOTTER* aPlotter ) override;

    EDA_ITEM* Clone() const override;

    void GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList ) override;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override;
#endif

    static void ShowSyntaxHelp( wxWindow* aParentWindow );
};


class SCH_LABEL : public SCH_TEXT
{
public:
    SCH_LABEL( const wxPoint& aPos = wxPoint( 0, 0 ), const wxString& aText = wxEmptyString );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_LABEL() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_LABEL_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_LABEL" );
    }

    bool IsType( const KICAD_T aScanTypes[] ) const override;

    const EDA_RECT GetBoundingBox() const override;

    bool IsConnectable() const override { return true; }

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS );
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    bool IsReplaceable() const override { return true; }

    EDA_ITEM* Clone() const override;

private:
    bool doIsConnected( const wxPoint& aPosition ) const override { return EDA_TEXT::GetTextPos() == aPosition; }
};


class SCH_GLOBALLABEL : public SCH_TEXT
{
public:
    SCH_GLOBALLABEL( const wxPoint& aPos = wxPoint( 0, 0 ), const wxString& aText = wxEmptyString );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_GLOBALLABEL() { }

    void Print( RENDER_SETTINGS* aSettings, const wxPoint& offset ) override;

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_GLOBAL_LABEL_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_GLOBALLABEL" );
    }

    void SetLabelSpinStyle( LABEL_SPIN_STYLE aSpinStyle ) override;

    wxPoint GetSchematicTextOffset( RENDER_SETTINGS* aSettings ) const override;

    const EDA_RECT GetBoundingBox() const override;

    void CreateGraphicShape( RENDER_SETTINGS* aRenderSettings,
                             std::vector<wxPoint>& aPoints, const wxPoint& aPos ) override;

    bool IsConnectable() const override { return true; }

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS );
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    EDA_ITEM* Clone() const override;

private:
    bool doIsConnected( const wxPoint& aPosition ) const override { return EDA_TEXT::GetTextPos() == aPosition; }
};


class SCH_HIERLABEL : public SCH_TEXT
{
public:
    SCH_HIERLABEL( const wxPoint& aPos = wxPoint( 0, 0 ), const wxString& aText = wxEmptyString,
                   KICAD_T aType = SCH_HIER_LABEL_T );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_HIERLABEL() { }

    void Print( RENDER_SETTINGS* aSettings, const wxPoint& offset ) override;

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_HIER_LABEL_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_HIERLABEL" );
    }

    void SetLabelSpinStyle( LABEL_SPIN_STYLE aSpinStyle ) override;

    wxPoint GetSchematicTextOffset( RENDER_SETTINGS* aSettings ) const override;

    void CreateGraphicShape( RENDER_SETTINGS* aSettings, std::vector<wxPoint>& aPoints,
                             const wxPoint& Pos ) override;

    const EDA_RECT GetBoundingBox() const override;

    bool IsConnectable() const override { return true; }

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS );
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override;

    BITMAP_DEF GetMenuImage() const override;

    EDA_ITEM* Clone() const override;

private:
    bool doIsConnected( const wxPoint& aPosition ) const override { return EDA_TEXT::GetTextPos() == aPosition; }
};

#endif /* CLASS_TEXT_LABEL_H */
