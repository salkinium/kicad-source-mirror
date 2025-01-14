/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2015 Jean-Pierre Charras, jaen-pierre.charras at wanadoo.fr
 * Copyright (C) 2015 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 2004-2019 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef _LIB_ITEM_H_
#define _LIB_ITEM_H_

#include <base_struct.h>
#include <eda_rect.h>
#include <transform.h>
#include <gr_basic.h>


class LINE_READER;
class OUTPUTFORMATTER;
class LIB_PART;
class PLOTTER;
class LIB_ITEM;
class LIB_PIN;
class MSG_PANEL_ITEM;


extern const int fill_tab[];


#define MINIMUM_SELECTION_DISTANCE 2 // Minimum selection distance in internal units


/**
 * Helper for defining a list of pin object pointers.  The list does not
 * use a Boost pointer class so the object pointers do not accidentally get
 * deleted when the container is deleted.
 */
typedef std::vector< LIB_PIN* > LIB_PINS;


/**
 * The base class for drawable items used by schematic library components.
 */
class LIB_ITEM : public EDA_ITEM
{
    /**
     * Print the item to \a aDC.
     *
     * @param aDC A pointer to the device context used to draw the object.
     * @param aOffset A reference to a wxPoint object containing the offset where to draw
     *                from the object's current position.
     * @param aData A pointer to any object specific data required to perform the draw.
     * @param aTransform A reference to a #TRANSFORM object containing drawing transform.
     */
    virtual void print( wxDC* aDC, const wxPoint& aOffset, void* aData,
                        const TRANSFORM& aTransform ) = 0;

    friend class LIB_PART;

protected:
    /**
     * Unit identification for multiple parts per package.  Set to 0 if the item is common
     * to all units.
     */
    int      m_Unit;

    /**
     * Shape identification for alternate body styles.  Set 0 if the item is common to all
     * body styles.  This is typially used for representing DeMorgan variants in KiCad.
     */
    int      m_Convert;

    /**
     * The body fill type.  This has meaning only for some items.  For a list of fill types
     * see #FILL_T.
     */
    FILL_T   m_Fill;
    bool     m_isFillable;

public:

    LIB_ITEM( KICAD_T aType, LIB_PART* aComponent = NULL, int aUnit = 0, int aConvert = 0,
              FILL_T aFillType = NO_FILL );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    virtual ~LIB_ITEM() { }


    // Define the enums for basic
    enum LIB_CONVERT : int  { BASE = 1, DEMORGAN = 2 };

    /**
     * Provide a user-consumable name of the object type.  Perform localization when
     * called so that run-time language selection works.
     */
    virtual wxString GetTypeName() = 0;

    /**
     * Begin drawing a component library draw item at \a aPosition.
     *
     * It typically would be called on a left click when a draw tool is selected in
     * the component library editor and one of the graphics tools is selected.
     *
     * @param aPosition The position in drawing coordinates where the drawing was started.
     *                  May or may not be required depending on the item being drawn.
     */
    virtual void BeginEdit( const wxPoint aPosition ) {}

    /**
     * Continue an edit in progress at \a aPosition.
     *
     * This is used to perform the next action while drawing an item.  This would be
     * called for each additional left click when the mouse is captured while the item
     * is being drawn.
     *
     * @param aPosition The position of the mouse left click in drawing coordinates.
     * @return True if additional mouse clicks are required to complete the edit in progress.
     */
    virtual bool ContinueEdit( const wxPoint aPosition ) { return false; }

    /**
     * End an object editing action.
     *
     * This is used to end or abort an edit action in progress initiated by BeginEdit().
     */
    virtual void EndEdit() {}

    /**
     * Calculates the attributes of an item at \a aPosition when it is being edited.
     *
     * This method gets called by the Draw() method when the item is being edited.  This
     * probably should be a pure virtual method but bezier curves are not yet editable in
     * the component library editor.  Therefore, the default method does nothing.
     *
     * @param aPosition The current mouse position in drawing coordinates.
     */
    virtual void CalcEdit( const wxPoint& aPosition ) {}


    /**
     * Draw an item
     *
     * @param aDC Device Context (can be null)
     * @param aOffset Offset to draw
     * @param aData Value or pointer used to pass others parameters, depending on body items.
     *              Used for some items to force to force no fill mode ( has meaning only for
     *              items what can be filled ). used in printing or moving objects mode or to
     *              pass reference to the lib component for pins.
     * @param aTransform Transform Matrix (rotation, mirror ..)
     */
    virtual void Print( wxDC* aDC, const wxPoint &aOffset, void* aData,
                        const TRANSFORM& aTransform );

    /**
     * @return the size of the "pen" that be used to draw or plot this item
     */
    virtual int GetPenSize() const = 0;

    LIB_PART* GetParent() const
    {
        return (LIB_PART*) m_Parent;
    }

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override
    {
        // This is just here to prevent annoying compiler warnings about hidden overloaded
        // virtual functions
        return EDA_ITEM::HitTest( aPosition, aAccuracy );
    }

    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    /**
     * @return the boundary box for this, in library coordinates
     */
    const EDA_RECT GetBoundingBox() const override { return EDA_ITEM::GetBoundingBox(); }

    /**
     * Display basic info (type, part and convert) about the current item in message panel.
     * <p>
     * This base function is used to display the information common to the
     * all library items.  Call the base class from the derived class or the
     * common information will not be updated in the message panel.
     * </p>
     * @param aList is the list to populate.
     */
    void GetMsgPanelInfo( EDA_UNITS aUnits, std::vector<MSG_PANEL_ITEM>& aList ) override;

    /**
     * Test LIB_ITEM objects for equivalence.
     *
     * @param aOther Object to test against.
     * @return True if object is identical to this object.
     */
    bool operator==( const LIB_ITEM& aOther ) const;
    bool operator==( const LIB_ITEM* aOther ) const
    {
        return *this == *aOther;
    }

    /**
     * Test if another draw item is less than this draw object.
     *
     * @param aOther - Draw item to compare against.
     * @return - True if object is less than this object.
     */
    bool operator<( const LIB_ITEM& aOther) const;

    /**
     * Set the drawing object by \a aOffset from the current position.
     *
     * @param aOffset Coordinates to offset the item position.
     */
    virtual void Offset( const wxPoint& aOffset ) = 0;

    /**
     * Test if any part of the draw object is inside rectangle bounds of \a aRect.
     *
     * @param aRect Rectangle to check against.
     * @return True if object is inside rectangle.
     */
    virtual bool Inside( EDA_RECT& aRect ) const = 0;

    /**
     * Move a draw object to \a aPosition.
     *
     * @param aPosition Position to move draw item to.
     */
    virtual void MoveTo( const wxPoint& aPosition ) = 0;

    virtual wxPoint GetPosition() const = 0;
    void SetPosition( const wxPoint& aPosition ) { MoveTo( aPosition ); }

    /**
     * Mirror the draw object along the horizontal (X) axis about \a aCenter point.
     *
     * @param aCenter Point to mirror around.
     */
    virtual void MirrorHorizontal( const wxPoint& aCenter ) = 0;

    /**
     * Mirror the draw object along the MirrorVertical (Y) axis about \a aCenter point.
     *
     * @param aCenter Point to mirror around.
     */
    virtual void MirrorVertical( const wxPoint& aCenter ) = 0;

    /**
     * Rotate the object about \a aCenter point.
     *
     * @param aCenter Point to rotate around.
     * @param aRotateCCW True to rotate counter clockwise.  False to rotate clockwise.
     */
    virtual void Rotate( const wxPoint& aCenter, bool aRotateCCW = true ) = 0;

    /**
     * Plot the draw item using the plot object.
     *
     * @param aPlotter The plot object to plot to.
     * @param aOffset Plot offset position.
     * @param aFill Flag to indicate whether or not the object is filled.
     * @param aTransform The plot transform.
     */
    virtual void Plot( PLOTTER* aPlotter, const wxPoint& aOffset, bool aFill,
                       const TRANSFORM& aTransform ) = 0;

    virtual int GetWidth() const = 0;
    virtual void SetWidth( int aWidth ) = 0;

    /**
     * Check if draw object can be filled.
     *
     * The default setting is false.  If the derived object support filling, set the
     * m_isFillable member to true.
     */
    bool IsFillable() const { return m_isFillable; }

    virtual COLOR4D GetDefaultColor();

    void SetUnit( int aUnit ) { m_Unit = aUnit; }
    int GetUnit() const { return m_Unit; }

    void SetConvert( int aConvert ) { m_Convert = aConvert; }
    int GetConvert() const { return m_Convert; }

    void SetFillMode( FILL_T aFillMode ) { m_Fill = aFillMode; }
    FILL_T GetFillMode() const { return m_Fill; }

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif

private:

    /**
     * Provide the draw object specific comparison called by the == and < operators.
     *
     * The base object sort order which always proceeds the derived object sort order
     * is as follows:
     *      - Component alternate part (DeMorgan) number.
     *      - Component part number.
     *      - KICAD_T enum value.
     *      - Result of derived classes comparison.
     *
     * @param aOther A reference to the other #LIB_ITEM to compare the arc against.
     * @return An integer value less than 0 if the object is less than \a aOther ojbect,
     *         zero if the object is equal to \a aOther object, or greater than 0 if the
     *         object is greater than \a aOther object.
     */
    virtual int compare( const LIB_ITEM& aOther ) const = 0;
};


#endif  //  _LIB_ITEM_H_
