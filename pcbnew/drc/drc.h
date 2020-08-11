/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2007-2016 Dick Hollenbeck, dick@softplc.com
 * Copyright (C) 2017-2019 KiCad Developers, see change_log.txt for contributors.
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

#ifndef DRC_H
#define DRC_H

#include <board_commit.h>
#include <class_board.h>
#include <class_track.h>
#include <class_marker_pcb.h>
#include <geometry/seg.h>
#include <geometry/shape_poly_set.h>
#include <memory>
#include <vector>
#include <tools/pcb_tool_base.h>


/// DRC error codes:
enum PCB_DRC_CODE {
    DRCE_FIRST = 1,
    DRCE_UNCONNECTED_ITEMS = DRCE_FIRST,    ///< items are unconnected
    DRCE_SHORTING_ITEMS,                    ///< items short two nets but are not a net tie
    DRCE_ALLOWED_ITEMS,                     ///< a disallowed item has been used
    DRCE_CLEARANCE,                         ///< items are too close together
    DRCE_TRACKS_CROSSING,                   ///< tracks are crossing
    DRCE_COPPER_EDGE_CLEARANCE,             ///< a copper item is too close to the board edge
    DRCE_ZONES_INTERSECT,                   ///< copper area outlines intersect
    DRCE_ZONE_HAS_EMPTY_NET,                ///< copper area has a net but no pads in nets, which is suspicious
    DRCE_DANGLING_VIA,                      ///< via which isn't connected to anything
    DRCE_DANGLING_TRACK,                    ///< track with at least one end not connected to anything
    DRCE_DRILLED_HOLES_TOO_CLOSE,           ///< overlapping drilled holes break drill bits
    DRCE_TRACK_WIDTH,                       ///< Track width is too small or too large
    DRCE_TOO_SMALL_VIA,                     ///< Too small via size
    DRCE_VIA_ANNULUS,                       ///< Via size and drill leave annulus too small or too large
    DRCE_TOO_SMALL_DRILL,                   ///< Too small via or pad drill
    DRCE_VIA_HOLE_BIGGER,                   ///< via's hole is bigger than its diameter
    DRCE_PADSTACK,                          ///< something is wrong with a pad or via stackup
    DRCE_TOO_SMALL_MICROVIA,                ///< Too small micro via size
    DRCE_TOO_SMALL_MICROVIA_DRILL,          ///< Too small micro via drill
    DRCE_KEEPOUT,                           ///< A disallowed object is inside a keepout
    DRCE_OVERLAPPING_FOOTPRINTS,            ///< footprint courtyards overlap
    DRCE_MISSING_COURTYARD,                 ///< footprint has no courtyard defined
    DRCE_MALFORMED_COURTYARD,               ///< footprint has a courtyard but malformed
                                            ///< (not convertible to a closed polygon with holes)
    DRCE_PTH_IN_COURTYARD,
    DRCE_NPTH_IN_COURTYARD,
    DRCE_DISABLED_LAYER_ITEM,               ///< item on a disabled layer
    DRCE_INVALID_OUTLINE,                   ///< invalid board outline
    DRCE_MISSING_FOOTPRINT,                 ///< footprint not found for netlist item
    DRCE_DUPLICATE_FOOTPRINT,               ///< more than one footprints found for netlist item
    DRCE_EXTRA_FOOTPRINT,                   ///< netlist item not found for footprint

    DRCE_UNRESOLVED_VARIABLE,

    DRCE_LAST = DRCE_UNRESOLVED_VARIABLE
};


class PCB_EDIT_FRAME;
class DIALOG_DRC;
class BOARD_ITEM;
class BOARD;
class D_PAD;
class ZONE_CONTAINER;
class TRACK;
class MARKER_PCB;
class DRC_ITEM;
class NETCLASS;
class EDA_TEXT;
class DRAWSEGMENT;
class NETLIST;
class wxWindow;
class wxString;
class wxTextCtrl;


/**
 * Design Rule Checker object that performs all the DRC tests.  The output of
 * the checking goes to the BOARD file in the form of two MARKER lists.  Those
 * two lists are displayable in the drc dialog box.  And they can optionally
 * be sent to a text file on disk.
 * This class is given access to the windows and the BOARD
 * that it needs via its constructor or public access functions.
 */
class DRC : public PCB_TOOL_BASE
{
    friend class DIALOG_DRC;

public:
    DRC();
    ~DRC();

    /// @copydoc TOOL_INTERACTIVE::Reset()
    void Reset( RESET_REASON aReason ) override;

private:
    bool     m_doUnconnectedTest;       // enable unconnected tests
    bool     m_testTracksAgainstZones;  // enable zone to items clearance tests
    bool     m_doKeepoutTest;           // enable keepout areas to items clearance tests
    bool     m_refillZones;             // refill zones if requested (by user).
    bool     m_reportAllTrackErrors;    // Report all tracks errors (or only 4 first errors)
    bool     m_testFootprints;          // Test footprints against schematic

    PCB_EDIT_FRAME*            m_editFrame;        // The pcb frame editor which owns the board
    BOARD*                     m_pcb;
    SHAPE_POLY_SET             m_board_outlines;   // The board outline including cutouts
    bool                       m_board_outline_valid;
    DIALOG_DRC*                m_drcDialog;

    std::vector<std::shared_ptr<DRC_ITEM> >     m_unconnected;      // list of unconnected pads
    std::vector<std::shared_ptr<DRC_ITEM> >     m_footprints;       // list of footprint warnings
    bool                       m_drcRun;           // indicates DRC has been run at least once
    bool                       m_footprintsTested; // indicates footprints were tested in last run

    std::vector<DRC_RULE*>     m_rules;

    // Temp variables for performance during a single DRC run
    //
    // wxString's c'tor is surprisingly expensive, and in the world of DRC everything matters
    //
    wxString                   m_msg;
    wxString                   m_clearanceSource;
    int                        m_largestClearance;

private:
    ///> Sets up handlers for various events.
    void setTransitions() override;

    /**
     * Update needed pointers from the one pointer which is known not to change.
     */
    void updatePointers();

    EDA_UNITS userUnits() const { return m_editFrame->GetUserUnits(); }

    /**
     * Adds a DRC marker to the PCB through the COMMIT mechanism.
     */
    void addMarkerToPcb( BOARD_COMMIT& aCommit, MARKER_PCB* aMarker );

    //-----<categorical group tests>-----------------------------------------

    /**
     * Perform the DRC on all tracks.
     *
     * This test can take a while, a progress bar can be displayed
     * @param aActiveWindow = the active window ued as parent for the progress bar
     * @param aShowProgressBar = true to show a progress bar
     * (Note: it is shown only if there are many tracks)
     */
    void testTracks( BOARD_COMMIT& aCommit, wxWindow * aActiveWindow, bool aShowProgressBar );

    void testPadClearances( BOARD_COMMIT& aCommit );

    void testUnconnected();

    void testZones( BOARD_COMMIT& aCommit );

    void testCopperDrawItem( BOARD_COMMIT& aCommit, BOARD_ITEM* aDrawing );

    void testCopperTextAndGraphics( BOARD_COMMIT& aCommit );

    // Tests for items placed on disabled layers (causing false connections).
    void testDisabledLayers( BOARD_COMMIT& aCommit );

    /**
     * Test that the board outline is contiguous and composed of valid elements
     */
    void testOutline( BOARD_COMMIT& aCommit );

    //-----<single "item" tests>-----------------------------------------

    /**
     * Test the clearance between aRefPad and other pads.
     *
     * The pad list must be sorted by x coordinate.
     *
     * @param aRefPad is the pad to test
     * @param aStart is the first pad of the list to test against aRefPad
     * @param aEnd is the end of the list and is not included
     * @param x_limit is used to stop the test
     * (i.e. when the current pad pos X in list exceeds this limit, because the list
     * is sorted by X coordinate)
     */
    bool doPadToPadsDrc( BOARD_COMMIT& aCommit, D_PAD* aRefPad, D_PAD** aStart, D_PAD** aEnd,
                         int x_limit );

    /**
     * Test the current segment.
     *
     * @param aRefSeg The segment to test
     * @param aStartIt the iterator to the first track to test
     * @param aEndIt the marker for the iterator end
     * @param aTestZones true if should do copper zones test. This can be very time consumming
     * @param aLayer sets the layer to test against
     * @return bool - true if no problems, else false and m_currentMarker is
     *          filled in with the problem information.
     */
    void doTrackDrc( BOARD_COMMIT& aCommit, TRACK* aRefSeg, TRACKS::iterator aStartIt,
                     TRACKS::iterator aEndIt, bool aTestZones, PCB_LAYER_ID aLayer );

    /**
     * Test a single via for DRC errors
     *
     * @param aCommit The board commit to add DRC errors
     * @param aRefVia The via to test against design settings
     */
    void doSingleViaDRC( BOARD_COMMIT& aCommit, VIA* aRefVia );

    /**
     * Test a single track segment for DRC errors
     *
     * @param aCommit The board commit to add DRC errors
     * @param aRefSeg The track to test against design settings
     */
    void doSingleTrackDRC( BOARD_COMMIT& aCommit, TRACK* aRefSeg );

public:
    /**
     * Load the DRC rules.  Must be called after the netclasses have been read.
     */
    bool LoadRules();

    /**
     * Fetches a reasonable point for marking a violoation between two non-point objects.
     */
    static wxPoint GetLocation( TRACK* aTrack, ZONE_CONTAINER* aConflictZone );
    static wxPoint GetLocation( TRACK* aTrack, const SEG& aConflictSeg );

    /**
     * Open a dialog and prompts the user, then if a test run button is
     * clicked, runs the test(s) and creates the MARKERS.  The dialog is only
     * created if it is not already in existence.
     *
     * @param aParent is the parent window for wxWidgets. Usually the PCB editor frame
     * but can be another dialog
     * if aParent == NULL (default), the parent will be the PCB editor frame
     * and the dialog will be not modal (just float on parent
     * if aParent is specified, the dialog will be modal.
     * The modal mode is mandatory if the dialog is created from another dialog, not
     * from the PCB editor frame
     */
    void ShowDRCDialog( wxWindow* aParent );

    int ShowDRCDialog( const TOOL_EVENT& aEvent );

    /**
     * Check to see if the DRC dialog is currently shown
     *
     * @return true if the dialog is shown
     */
    bool IsDRCDialogShown();

    /**
     * Deletes this ui dialog box and zeros out its pointer to remember
     * the state of the dialog's existence.
     *
     * @param aReason Indication of which button was clicked to cause the destruction.
     * if aReason == wxID_OK, design parameters values which can be entered from the dialog
     * will bbe saved in design parameters list
     */
    void DestroyDRCDialog( int aReason );

    /**
     * Run all the tests specified with a previous call to
     * SetSettings()
     * @param aMessages = a wxTextControl where to display some activity messages. Can be NULL
     */
    void RunTests( wxTextCtrl* aMessages = NULL );
};


#endif  // DRC_H
