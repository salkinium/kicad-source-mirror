/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2012 Torsten Hueter, torstenhtr <at> gmx.de
 * Copyright (C) 2013 CERN
 * Copyright (C) 2013-2019 KiCad Developers, see AUTHORS.txt for contributors.
 *
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
 *
 */

/**
 * @file view_controls.h
 * @brief VIEW_CONTROLS class definition.
 */

#ifndef __VIEW_CONTROLS_H
#define __VIEW_CONTROLS_H

#include <math/box2.h>

namespace KIGFX
{
class VIEW;

///> Structure to keep VIEW_CONTROLS settings for easy store/restore operations
struct VC_SETTINGS
{
    VC_SETTINGS()
    {
        Reset();
    }

    ///> Restores the default settings
    void Reset();

    ///> Flag determining the cursor visibility
    bool m_showCursor;

    ///> Forced cursor position (world coordinates)
    VECTOR2D m_forcedPosition;

    ///> Is the forced cursor position enabled
    bool m_forceCursorPosition;

    ///> Should the cursor be locked within the parent window area
    bool m_cursorCaptured;

    ///> Should the cursor snap to grid or move freely
    bool m_snappingEnabled;

    ///> Flag for grabbing the mouse cursor
    bool m_grabMouse;

    ///> Flag for turning on autopanning
    bool m_autoPanEnabled;

    ///> Flag for turning on autopanning
    bool m_autoPanSettingEnabled;

    ///> Distance from cursor to VIEW edge when panning is active
    float m_autoPanMargin;

    ///> How fast is panning when in auto mode
    float m_autoPanSpeed;

    ///> If the cursor is allowed to be warped
    bool m_warpCursor;

    ///> Mousewheel (2-finger touchpad) panning
    bool m_enableMousewheelPan;

    ///> Allow panning with the right button in addition to middle
    bool m_panWithRightButton;

    ///> Allow panning with the left button in addition to middle
    bool m_panWithLeftButton;

    ///> Is last cursor motion event coming from keyboard arrow cursor motion action
    bool m_lastKeyboardCursorPositionValid;

    ///> ACTIONS::CURSOR_UP, ACTIONS::CURSOR_DOWN, etc.
    long m_lastKeyboardCursorCommand;

    ///> Position of the above event
    VECTOR2D m_lastKeyboardCursorPosition;
};


/**
 * Class VIEW_CONTROLS
 * is an interface for classes handling user events controlling the view behaviour
 * (such as zooming, panning, mouse grab, etc.)
 */
class VIEW_CONTROLS
{
public:
    VIEW_CONTROLS( VIEW* aView ) :
        m_view( aView ), m_cursorWarped( false )
    {
    }

    virtual ~VIEW_CONTROLS()
    {
    }

    /**
     * Function SetSnapping()
     * Enables/disables snapping cursor to grid.
     *
     * @param aEnabled says whether the opion should be enabled or disabled.
     */
    virtual void SetSnapping( bool aEnabled )
    {
        m_settings.m_snappingEnabled = aEnabled;
    }

    /**
     * @return the current state of the snapping cursor to grid.
     */
    virtual bool GetSnappingState()
    {
        return m_settings.m_snappingEnabled;
    }

    /**
     * Function SetGrabMouse
     * Turns on/off mouse grabbing. When the mouse is grabbed, it cannot go outside the VIEW.
     * @param aEnabled tells if mouse should be grabbed or not.
     */
    virtual void SetGrabMouse( bool aEnabled )
    {
        m_settings.m_grabMouse = aEnabled;
    }

    /**
     * Function SetAutoPan
     * Turns on/off auto panning (this feature is used when there is a tool active (eg. drawing a
     * track) and user moves mouse to the VIEW edge - then the view can be translated or not).
     * @param aEnabled tells if the autopanning should be active.
     */
    virtual void SetAutoPan( bool aEnabled )
    {
        m_settings.m_autoPanEnabled = aEnabled;
    }

    /**
     * Function EnableAutoPan
     * Turns on/off auto panning (user setting to disable it entirely).
     * @param aEnabled tells if the autopanning should be enabled.
     */
    virtual void EnableAutoPan( bool aEnabled )
    {
        m_settings.m_autoPanSettingEnabled = aEnabled;
    }

    /**
     * Function SetAutoPanSpeed()
     * Sets speed of autopanning.
     * @param aSpeed is a new speed for autopanning.
     */
    virtual void SetAutoPanSpeed( float aSpeed )
    {
        m_settings.m_autoPanSpeed = aSpeed;
    }

    /**
     * Function SetAutoPanMArgin()
     * Sets margin for autopanning (ie. the area when autopanning becomes active).
     * @param aMargin is a new margin for autopanning.
     */
    virtual void SetAutoPanMargin( float aMargin )
    {
        m_settings.m_autoPanMargin = aMargin;
    }

    /**
     * Function GetMousePosition()
     * Returns the current mouse pointer position. Note, that it may be
     * different from the cursor position if snapping is enabled (@see GetCursorPosition()).
     *
     * @param aWorldCoordinates if true, the result is given in world coordinates, otherwise
     * it is given in screen coordinates.
     * @return The current mouse pointer position in either world or screen coordinates.
     */
    virtual VECTOR2D GetMousePosition( bool aWorldCoordinates = true ) const = 0;

    /**
     * Returns the current cursor position in world coordinates. Note, that it may be
     * different from the mouse pointer position if snapping is enabled or cursor position
     * is forced to a specific point.
     *
     * @return The current cursor position in world coordinates.
     */
    VECTOR2D GetCursorPosition() const
    {
        return GetCursorPosition( m_settings.m_snappingEnabled );
    }

    /**
     * Returns the current cursor position in world coordinates - ingoring the cursorUp
     * position force mode.
     *
     * @return The current cursor position in world coordinates.
     */
    virtual VECTOR2D GetRawCursorPosition( bool aSnappingEnabled = true ) const = 0;

    /**
     * Returns the current cursor position in world coordinates. Note, that it may be
     * different from the mouse pointer position if snapping is enabled or cursor position
     * is forced to a specific point.
     *
     * @param aEnableSnapping selects whether cursor position should be snapped to the grid.
     * @return The current cursor position in world coordinates.
     */
    virtual VECTOR2D GetCursorPosition( bool aEnableSnapping ) const = 0;

    /**
     * Function ForceCursorPosition()
     * Places the cursor immediately at a given point. Mouse movement is ignored.
     * @param aEnabled enable forced cursor position
     * @param aPosition the position (world coordinates).
     */
    virtual void ForceCursorPosition( bool aEnabled, const VECTOR2D& aPosition = VECTOR2D( 0, 0 ) )
    {
        m_settings.m_forceCursorPosition = aEnabled;
        m_settings.m_forcedPosition = aPosition;
    }

    /**
     * Moves cursor to the requested position expressed in world coordinates. The position is not
     * forced and will be overridden with the next mouse motion event. Mouse cursor follows the
     * world cursor.
     * @param aPosition is the requested cursor position in the world coordinates.
     * @param aWarpView enables/disables view warp if the cursor is outside the current viewport.
     */
    virtual void SetCursorPosition( const VECTOR2D& aPosition, bool aWarpView = true,
                                    bool aTriggeredByArrows = false, long aArrowCommand = 0 ) = 0;


    /**
     * Moves the graphic crosshair cursor to the requested position expressed in world coordinates.
     * @param aPosition is the requested cursor position in the world coordinates.
     * @param aWarpView enables/disables view warp if the cursor is outside the current viewport.
     */
    virtual void SetCrossHairCursorPosition( const VECTOR2D& aPosition, bool aWarpView = true ) = 0;


    /**
     * Function ForcedCursorPosition()
     * Returns true if the current cursor position is forced to a specific location, ignoring
     * the mouse cursor position.
     */
    bool ForcedCursorPosition() const
    {
        return m_settings.m_forceCursorPosition;
    }

    /**
     * Function ShowCursor()
     * Enables or disables display of cursor.
     * @param aEnabled decides if the cursor should be shown.
     */
    virtual void ShowCursor( bool aEnabled );

    /**
     * Function IsCursorShown()
     * Returns true when cursor is visible.
     * @return True if cursor is visible.
     */
    bool IsCursorShown() const;

    /**
     * Function CaptureCursor()
     * Forces the cursor to stay within the drawing panel area.
     * @param aEnabled determines if the cursor should be captured.
     */
    virtual void CaptureCursor( bool aEnabled )
    {
        m_settings.m_cursorCaptured = aEnabled;
    }

    /**
     * Function IsCursorPositionForced()
     * Returns true if the cursor position is set by one of the tools. Forced cursor position
     * means it does not react to mouse movement.
     */
    inline bool IsCursorPositionForced() const
    {
        return m_settings.m_forceCursorPosition;
    }

    /**
     * Function WarpCursor()
     * If enabled (@see SetEnableCursorWarping(), warps the cursor to the specified position,
     * expressed either in the screen coordinates or the world coordinates.
     * @param aPosition is the position where the cursor should be warped.
     * @param aWorldCoordinates if true treats aPosition as the world coordinates, otherwise it
     * uses it as the screen coordinates.
     * @param aWarpView determines if the view can be warped too (only matters if the position is
     * specified in the world coordinates and its not visible in the current viewport).
     */
    virtual void WarpCursor( const VECTOR2D& aPosition, bool aWorldCoordinates = false,
            bool aWarpView = false ) = 0;

    /**
     * Function EnableCursorWarping()
     * Enables or disables warping the cursor.
     * @param aEnable is true if the cursor is allowed to be warped.
     */
    void EnableCursorWarping( bool aEnable )
    {
        m_settings.m_warpCursor = aEnable;
    }

    /**
     * Function IsCursorWarpingEnabled()
     * @return the current setting for cursor warping.
     */
    bool IsCursorWarpingEnabled() const
    {
        return m_settings.m_warpCursor;
    }

    /**
     * Function EnableMousewheelPan()
     * Enables or disables mousewheel panning.
     * @param aEnable is true if mouse-wheel panning is enabled.
     */
    virtual void EnableMousewheelPan( bool aEnable )
    {
        m_settings.m_enableMousewheelPan = aEnable;
    }

    /**
     * Function IsMousewheelPanEnabled()
     * @return the current setting for mousewheel panning
     */
    virtual bool IsMousewheelPanEnabled() const
    {
        return m_settings.m_enableMousewheelPan;
    }

    /**
     * Function CenterOnCursor()
     * Sets the viewport center to the current cursor position and warps the cursor to the
     * screen center.
     */
    virtual void CenterOnCursor() const = 0;

    void SetAdditionalPanButtons( bool aLeft = false, bool aRight = false )
    {
        m_settings.m_panWithLeftButton = aLeft;
        m_settings.m_panWithRightButton = aRight;
    }

    /**
     * Function Reset()
     * Restores the default VIEW_CONTROLS settings.
     */
    virtual void Reset();

    ///> Returns the current VIEW_CONTROLS settings
    const VC_SETTINGS& GetSettings() const
    {
        return m_settings;
    }

    ///> Applies VIEW_CONTROLS settings from an object
    void ApplySettings( const VC_SETTINGS& aSettings );

protected:
    ///> Pointer to controlled VIEW.
    VIEW* m_view;

    ///> Application warped the cursor, not the user (keyboard)
    bool m_cursorWarped;

    ///> Current VIEW_CONTROLS settings
    VC_SETTINGS m_settings;
};
} // namespace KIGFX

#endif
