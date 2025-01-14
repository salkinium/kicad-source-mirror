/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2009 Jean-Pierre Charras, jaen-pierre.charras@gipsa-lab.inpg.com
 * Copyright (C) 1992-2019 KiCad Developers, see AUTHORS.txt for contributors.
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

#ifndef _SCH_JUNCTION_H_
#define _SCH_JUNCTION_H_


#include <sch_item.h>
class NETLIST_OBJECT_LIST;

class SCH_JUNCTION : public SCH_ITEM
{
    wxPoint m_pos;                  // Position of the junction.
    static int m_symbolSize;        // diameter of the junction graphic symbol

public:
    SCH_JUNCTION( const wxPoint& pos = wxPoint( 0, 0 ) );

    // Do not create a copy constructor.  The one generated by the compiler is adequate.

    ~SCH_JUNCTION() { }

    static inline bool ClassOf( const EDA_ITEM* aItem )
    {
        return aItem && SCH_JUNCTION_T == aItem->Type();
    }

    wxString GetClass() const override
    {
        return wxT( "SCH_JUNCTION" );
    }

    static int GetSymbolSize() { return m_symbolSize; }
    static void SetSymbolSize( int aSize ) { m_symbolSize = aSize; }

    // Return the size the symbol should be drawn at.  This is GetSymbolSize() clamped to be
    // no less than the current wire width.
    static int GetEffectiveSymbolSize();

    void SwapData( SCH_ITEM* aItem ) override;

    void ViewGetLayers( int aLayers[], int& aCount ) const override;

    const EDA_RECT GetBoundingBox() const override;

    void Print( wxDC* aDC, const wxPoint& aOffset ) override;

    void Move( const wxPoint& aMoveVector ) override
    {
        m_pos += aMoveVector;
    }

    void MirrorY( int aYaxis_position ) override;
    void MirrorX( int aXaxis_position ) override;
    void Rotate( wxPoint aPosition ) override;

    void GetEndPoints( std::vector <DANGLING_END_ITEM>& aItemList ) override;

    bool IsConnectable() const override { return true; }

    void GetConnectionPoints( std::vector< wxPoint >& aPoints ) const override;

    bool CanConnect( const SCH_ITEM* aItem ) const override
    {
        return ( aItem->Type() == SCH_LINE_T &&
                ( aItem->GetLayer() == LAYER_WIRE || aItem->GetLayer() == LAYER_BUS ) ) ||
                aItem->Type() == SCH_COMPONENT_T;
    }

    wxString GetSelectMenuText( EDA_UNITS aUnits ) const override
    {
        return wxString( _( "Junction" ) );
    }

    BITMAP_DEF GetMenuImage() const override;

    void GetNetListItem( NETLIST_OBJECT_LIST& aNetListItems, SCH_SHEET_PATH* aSheetPath ) override;

    wxPoint GetPosition() const override { return m_pos; }
    void SetPosition( const wxPoint& aPosition ) override { m_pos = aPosition; }

    bool HitTest( const wxPoint& aPosition, int aAccuracy = 0 ) const override;
    bool HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy = 0 ) const override;

    void Plot( PLOTTER* aPlotter ) override;

    EDA_ITEM* Clone() const override;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override;
#endif

private:
    bool doIsConnected( const wxPoint& aPosition ) const override;
};


#endif    // _SCH_JUNCTION_H_
