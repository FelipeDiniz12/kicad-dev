/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015-2017 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
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

class SELECTION;
class COMMIT;

class PCB_BASE_FRAME;

#include <wx_unit_binder.h>
#include <dialogs/dialog_graphic_arc_properties_base.h>

class DIALOG_GRAPHIC_ARC_PROPERTIES : public DIALOG_GRAPHIC_ARC_PROPERTIES_BASE
{
public:

    enum ARC_DEFINITION_MODE
    {
        ADM_BY_ENDPOINTS = 0,
        ADM_BY_ANGLES = 1
    };

    DIALOG_GRAPHIC_ARC_PROPERTIES( PCB_BASE_FRAME* aParent, const SELECTION& aItems );

    ///> Applies values from the dialog to the selected items.
    bool Apply( COMMIT& aCommit );

private:
    void onClose( wxCloseEvent& aEvent ) override;
    void onCancelClick( wxCommandEvent& aEvent ) override;
    void onOkClick( wxCommandEvent& aEvent ) override;

    void OnInitDlg( wxInitDialogEvent& event ) override
    {
        // Call the default wxDialog handler of a wxInitDialogEvent
        TransferDataToWindow();

        // Now all widgets have the size fixed, call FinishDialogSettings
        FinishDialogSettings();
    }

    virtual void onDefineAsCoords( wxCommandEvent& event ) override;
    virtual void onDefineAsAngleRadius( wxCommandEvent& event ) override;


    ///> Checks if the dialog values are correct.
    bool check() const;

    ///> Selected items to be modified.
    const SELECTION& m_items;

    void setDefinitionMode( ARC_DEFINITION_MODE aMode );

    ARC_DEFINITION_MODE m_definitionMode;

    WX_UNIT_BINDER m_startX, m_startY;
    WX_UNIT_BINDER m_endX, m_endY;
    WX_UNIT_BINDER m_centerX, m_centerY;
    WX_UNIT_BINDER m_radius;
    WX_UNIT_BINDER m_width;
    WX_UNIT_BINDER m_startAngle, m_centralAngle;


};
