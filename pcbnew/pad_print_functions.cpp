/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2012 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2012 Wayne Stambaugh <stambaughw@verizon.net>
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

#include <class_board.h>
#include <common.h>
#include <convert_basic_shapes_to_polygon.h>
#include <gr_text.h>
#include <fctsys.h>
#include <geometry/geometry_utils.h>
#include <gr_basic.h>
#include <layers_id_colors_and_visibility.h>
#include <pcb_edit_frame.h>
#include <pcb_screen.h>
#include <pcbnew.h>
#include <pcbnew_id.h> // ID_TRACK_BUTT
#include <trigo.h>


// Helper class to store parameters used to draw a pad
PAD_DRAWINFO::PAD_DRAWINFO()
{
    m_Color           = BLACK;
    m_HoleColor       = BLACK; // could be DARKGRAY;
    m_NPHoleColor     = YELLOW;
    m_NoNetMarkColor  = BLUE;
    m_PadClearance    = 0;
    m_Display_padnum  = true;
    m_Display_netname = true;
    m_ShowPadFilled   = true;
    m_ShowNCMark      = true;
    m_ShowNotPlatedHole = false;
    m_IsPrinting = false;
}


void D_PAD::Print( PCB_BASE_FRAME* aFrame, wxDC* aDC, const wxPoint& aOffset )
{
    wxSize mask_margin;   // margin (clearance) used for some non copper layers

    if( m_Flags & DO_NOT_DRAW )
        return;

    PAD_DRAWINFO drawInfo;

    drawInfo.m_Offset = aOffset;

    /* We can show/hide pads from the layer manager.
     * options are show/hide pads on front and/or back side of the board
     * For through pads, we hide them only if both sides are hidden.
     * smd pads on back are hidden for all layers (copper and technical layers)
     * on back side of the board
     * smd pads on front are hidden for all layers (copper and technical layers)
     * on front side of the board
     * ECO, edge and Draw layers and not considered
     */

    BOARD* brd = GetBoard();

    const auto& cds = aFrame->Settings().Colors();

    bool   frontVisible = brd->IsElementVisible( LAYER_PAD_FR );
    bool   backVisible  = brd->IsElementVisible( LAYER_PAD_BK );

    if( !frontVisible && !backVisible )
        return;

    // If pad is only on front side (no layer on back side)
    // and if hide front side pads is enabled, do not draw
    if( !frontVisible && !( m_layerMask & LSET::BackMask() ).any() )
        return;

    // If pad is only on back side (no layer on front side)
    // and if hide back side pads is enabled, do not draw
    if( !backVisible && !( m_layerMask & LSET::FrontMask() ).any() )
        return;


    auto& displ_opts = aFrame->GetDisplayOptions();
    PCB_SCREEN* screen = aFrame->GetScreen();

    if( displ_opts.m_DisplayPadFill == SKETCH )
        drawInfo.m_ShowPadFilled = false;
    else
        drawInfo.m_ShowPadFilled = true;

    COLOR4D color = COLOR4D::BLACK;

    if( m_layerMask[F_Cu] )
    {
        color = cds.GetItemColor( LAYER_PAD_FR );
    }

    if( m_layerMask[B_Cu] )
    {
        color = color.LegacyMix( cds.GetItemColor( LAYER_PAD_BK ) );
    }

    if( color == BLACK ) // Not on a visible copper layer (i.e. still nothing to show)
    {
        // If the pad is on only one tech layer, use the layer color else use DARKGRAY
        LSET mask_non_copper_layers = m_layerMask & ~LSET::AllCuMask();

        PCB_LAYER_ID pad_layer = mask_non_copper_layers.ExtractLayer();

        switch( (int) pad_layer )
        {
        case UNDEFINED_LAYER:   // More than one layer
            color = DARKGRAY;
            break;

        case UNSELECTED_LAYER:  // Shouldn't really happen...
            break;

        default:
            color = cds.GetLayerColor( pad_layer );
        }
    }

    if( ( IsOnLayer( B_Paste ) && brd->IsLayerVisible( B_Paste ) ) ||
        ( IsOnLayer( F_Paste ) && brd->IsLayerVisible( F_Paste ) ) )
    {
        mask_margin = GetSolderPasteMargin();
    }

    if( ( IsOnLayer( B_Mask ) && brd->IsLayerVisible( B_Mask ) ) ||
        ( IsOnLayer( F_Mask ) && brd->IsLayerVisible( F_Mask ) ) )
    {
        mask_margin.x = std::max( mask_margin.x, GetSolderMaskMargin() );
        mask_margin.y = std::max( mask_margin.y, GetSolderMaskMargin() );
    }

    bool DisplayIsol = displ_opts.m_DisplayPadIsol;

    if( !( m_layerMask & LSET::AllCuMask() ).any() )
        DisplayIsol = false;

    if( ( GetAttribute() == PAD_ATTRIB_HOLE_NOT_PLATED ) &&
        brd->IsElementVisible( LAYER_NON_PLATEDHOLES ) )
    {
        drawInfo.m_ShowNotPlatedHole = true;
        drawInfo.m_NPHoleColor = cds.GetItemColor( LAYER_NON_PLATEDHOLES );
    }
    // Don't let pads that *should* be NPTHs get lost
    else if ( PadShouldBeNPTH() )
    {
        drawInfo.m_ShowNotPlatedHole = true;
        drawInfo.m_NPHoleColor = cds.GetItemColor( LAYER_MOD_TEXT_INVISIBLE );
    }

    drawInfo.m_Color          = color;
    drawInfo.m_NoNetMarkColor = cds.GetItemColor( LAYER_NO_CONNECTS );
    drawInfo.m_Mask_margin    = mask_margin;
    drawInfo.m_ShowNCMark     = brd->IsElementVisible( LAYER_NO_CONNECTS );
    drawInfo.m_IsPrinting     = screen->m_IsPrinting;
    color.a = 0.666;

    /* Get the pad clearance. This has a meaning only for Pcbnew.
     *  for CvPcb GetClearance() creates debug errors because
     *  there is no net classes so a call to GetClearance() is made only when
     *   needed (never needed in CvPcb)
     */
    drawInfo.m_PadClearance = DisplayIsol ? GetClearance() : 0;

    // Draw the pad number
    if( !displ_opts.m_DisplayPadNum )
        drawInfo.m_Display_padnum = false;

    if( ( displ_opts.m_DisplayNetNamesMode == 0 ) || ( displ_opts.m_DisplayNetNamesMode == 2 ) )
        drawInfo.m_Display_netname = false;

    PrintShape( aDC, drawInfo );
}


void D_PAD::PrintShape( wxDC* aDC, PAD_DRAWINFO& aDrawInfo )
{
    #define SEGCOUNT 32     // number of segments to approximate a circle
    wxPoint coord[12];
    double  angle = m_Orient;
    int     seg_width;

    // calculate pad shape position :
    wxPoint shape_pos = ShapePos() - aDrawInfo.m_Offset;

    wxSize  halfsize = m_Size;
    halfsize.x >>= 1;
    halfsize.y >>= 1;

    switch( GetShape() )
    {
    case PAD_SHAPE_CIRCLE:
        if( aDrawInfo.m_ShowPadFilled )
            GRFilledCircle( nullptr, aDC, shape_pos.x, shape_pos.y,
                            halfsize.x + aDrawInfo.m_Mask_margin.x, 0,
                            aDrawInfo.m_Color, aDrawInfo.m_Color );
        else
            GRCircle( nullptr, aDC, shape_pos.x, shape_pos.y,
                      halfsize.x + aDrawInfo.m_Mask_margin.x,
                      m_PadSketchModePenSize, aDrawInfo.m_Color );

        if( aDrawInfo.m_PadClearance )
        {
            GRCircle( nullptr, aDC, shape_pos.x, shape_pos.y,
                      halfsize.x + aDrawInfo.m_PadClearance, 0, aDrawInfo.m_Color );
        }

        break;

    case PAD_SHAPE_OVAL:
    {
        wxPoint segStart, segEnd;
        seg_width = BuildSegmentFromOvalShape( segStart, segEnd, angle, aDrawInfo.m_Mask_margin );
        segStart += shape_pos;
        segEnd += shape_pos;

        if( aDrawInfo.m_ShowPadFilled )
        {
            GRFillCSegm( nullptr, aDC, segStart.x, segStart.y, segEnd.x, segEnd.y, seg_width,
                         aDrawInfo.m_Color );
        }
        else
        {
            GRCSegm( nullptr, aDC, segStart.x, segStart.y, segEnd.x, segEnd.y, seg_width,
                     m_PadSketchModePenSize, aDrawInfo.m_Color );
        }

        // Draw the clearance line
        if( aDrawInfo.m_PadClearance )
        {
            seg_width += 2 * aDrawInfo.m_PadClearance;
            GRCSegm( nullptr, aDC, segStart.x, segStart.y, segEnd.x, segEnd.y, seg_width,
                     aDrawInfo.m_Color );
        }
    }
        break;

    case PAD_SHAPE_RECT:
    case PAD_SHAPE_TRAPEZOID:
        BuildPadPolygon( coord, aDrawInfo.m_Mask_margin, angle );

        for( int ii = 0; ii < 4; ii++ )
            coord[ii] += shape_pos;

        GRClosedPoly( nullptr, aDC, 4, coord, aDrawInfo.m_ShowPadFilled,
                      aDrawInfo.m_ShowPadFilled ? 0 : m_PadSketchModePenSize,
                      aDrawInfo.m_Color, aDrawInfo.m_Color );

        if( aDrawInfo.m_PadClearance )
        {
            SHAPE_POLY_SET outline;
            TransformShapeWithClearanceToPolygon( outline, aDrawInfo.m_PadClearance );

            // Draw the polygon: Inflate creates only one convex polygon
            if( outline.OutlineCount() > 0 )
            {
                SHAPE_LINE_CHAIN& poly = outline.Outline( 0 );

                if( poly.PointCount() > 0 )
                {
                    GRClosedPoly( nullptr, aDC, poly.PointCount(),
                            (const wxPoint*) &poly.CPoint( 0 ), false, 0, aDrawInfo.m_Color,
                            aDrawInfo.m_Color );
                }
            }
        }
        break;

    case PAD_SHAPE_CHAMFERED_RECT:
    case PAD_SHAPE_ROUNDRECT:
        {
        // Use solder[Paste/Mask]size or pad size to build pad shape to draw
        wxSize size( GetSize() );
        size += aDrawInfo.m_Mask_margin * 2;
        int corner_radius = GetRoundRectCornerRadius( size );
        bool doChamfer = GetShape() == PAD_SHAPE_CHAMFERED_RECT;

        SHAPE_POLY_SET outline;
        TransformRoundChamferedRectToPolygon( outline, shape_pos, size, GetOrientation(),
                                     corner_radius, GetChamferRectRatio(),
                                     doChamfer ? GetChamferPositions() : 0,
                                     ARC_HIGH_DEF );

        // Draw the polygon: Inflate creates only one convex polygon
        bool filled = aDrawInfo.m_ShowPadFilled;

        SHAPE_LINE_CHAIN& poly = outline.Outline( 0 );

        GRClosedPoly( nullptr, aDC, poly.PointCount(), (const wxPoint*) &poly.CPoint( 0 ), filled,
                0, aDrawInfo.m_Color, aDrawInfo.m_Color );

        if( aDrawInfo.m_PadClearance )
        {
            outline.RemoveAllContours();
            size = GetSize();
            size.x += aDrawInfo.m_PadClearance * 2;
            size.y += aDrawInfo.m_PadClearance * 2;
            corner_radius = GetRoundRectCornerRadius() + aDrawInfo.m_PadClearance;

            TransformRoundChamferedRectToPolygon( outline, shape_pos, size, GetOrientation(),
                                         corner_radius, GetChamferRectRatio(),
                                         doChamfer ? GetChamferPositions() : 0,
                                         ARC_HIGH_DEF );

            // Draw the polygon: Inflate creates only one convex polygon
            SHAPE_LINE_CHAIN& clearance_poly = outline.Outline( 0 );

            GRClosedPoly( nullptr, aDC, clearance_poly.PointCount(),
                    (const wxPoint*) &clearance_poly.CPoint( 0 ), false, 0, aDrawInfo.m_Color,
                    aDrawInfo.m_Color );
        }
        }
        break;

    case PAD_SHAPE_CUSTOM:
        {
        // The full shape has 2 items
        // 1- The anchor pad: a round or rect pad located at pad position
        // 2- The custom complex shape
        // Note: The anchor pad shape is containing by the custom complex shape polygon
        // The anchor pad is shown to help user to see where is the anchor, only in sketch mode
        // (In filled mode, it is merged with the basic shapes)
        wxPoint pad_pos = GetPosition() - aDrawInfo.m_Offset;

        // In sketch mode only: Draw the anchor pad: a round or rect pad
        if( !aDrawInfo.m_ShowPadFilled )
        {
            if( GetAnchorPadShape() == PAD_SHAPE_RECT )
            {
                wxPoint poly[4];
                poly[0] = wxPoint( - halfsize.x, - halfsize.y );
                poly[1] = wxPoint( - halfsize.x, + halfsize.y );
                poly[2] = wxPoint( + halfsize.x, + halfsize.y );
                poly[3] = wxPoint( + halfsize.x, - halfsize.y );

                for( int ii = 0; ii < 4; ++ii )
                {
                    RotatePoint( &poly[ii], m_Orient );
                    poly[ii] += pad_pos;
                }

                GRClosedPoly( nullptr, aDC, 4, poly, false, 0, aDrawInfo.m_Color,
                              aDrawInfo.m_Color );
            }
            else
            {
                GRCircle( nullptr, aDC, pad_pos.x, pad_pos.y, halfsize.x,
                          m_PadSketchModePenSize, aDrawInfo.m_Color );
            }
        }

        SHAPE_POLY_SET outline;     // Will contain the corners in board coordinates
        outline.Append( m_customShapeAsPolygon );
        CustomShapeAsPolygonToBoardPosition( &outline, pad_pos, GetOrientation() );

        if( aDrawInfo.m_Mask_margin.x )
        {
            int numSegs = GetArcToSegmentCount( aDrawInfo.m_Mask_margin.x, ARC_HIGH_DEF, 360.0 );
            outline.InflateWithLinkedHoles(
                    aDrawInfo.m_Mask_margin.x, numSegs, SHAPE_POLY_SET::PM_FAST );
        }

        // Draw the polygon: only one polygon is expected
        // However we provide a multi polygon shape drawing
        // ( can happen with CUSTOM pads and negative margins )
        for( int jj = 0; jj < outline.OutlineCount(); ++jj )
        {
            auto& poly = outline.Outline( jj );

            GRClosedPoly( nullptr, aDC, poly.PointCount(), (const wxPoint*) &poly.CPoint( 0 ),
                    aDrawInfo.m_ShowPadFilled, 0, aDrawInfo.m_Color, aDrawInfo.m_Color );
        }

        if( aDrawInfo.m_PadClearance )
        {
            SHAPE_POLY_SET clearance_outline;
            clearance_outline.Append( outline );

            int numSegs = GetArcToSegmentCount( aDrawInfo.m_PadClearance, ARC_HIGH_DEF, 360.0 );
            clearance_outline.InflateWithLinkedHoles(
                    aDrawInfo.m_PadClearance, numSegs, SHAPE_POLY_SET::PM_FAST );

            for( int jj = 0; jj < clearance_outline.OutlineCount(); ++jj )
            {
                auto& poly = clearance_outline.Outline( jj );

                if( poly.PointCount() > 0 )
                {
                    GRClosedPoly( nullptr, aDC, poly.PointCount(),
                            (const wxPoint*) &poly.CPoint( 0 ), false, 0, aDrawInfo.m_Color,
                            aDrawInfo.m_Color );
                }
            }
        }

        break;
        }
    }

    // Draw the pad hole
    wxPoint holepos = m_Pos - aDrawInfo.m_Offset;
    int     hole    = m_Drill.x >> 1;

    bool drawhole = hole > 0;

    if( !aDrawInfo.m_ShowPadFilled && !aDrawInfo.m_ShowNotPlatedHole )
        drawhole = false;

    if( drawhole )
    {
        bool blackpenstate = false;
        COLOR4D fillcolor = aDrawInfo.m_ShowNotPlatedHole? aDrawInfo.m_NPHoleColor :
                                                           aDrawInfo.m_HoleColor;
        COLOR4D hole_color = fillcolor;

        fillcolor = COLOR4D::WHITE;
        blackpenstate = GetGRForceBlackPenState();
        GRForceBlackPen( false );

        if( blackpenstate )
            hole_color = COLOR4D::BLACK;

        switch( GetDrillShape() )
        {
        case PAD_DRILL_SHAPE_CIRCLE:
            if( aDC->LogicalToDeviceXRel( hole ) > 1 )  // hole is drawn if hole > 1pixel
                GRFilledCircle( nullptr, aDC, holepos.x, holepos.y, hole, 0, hole_color, fillcolor );
            break;

        case PAD_DRILL_SHAPE_OBLONG:
        {
            wxPoint drl_start, drl_end;
            GetOblongDrillGeometry( drl_start, drl_end, seg_width );
            drl_start += holepos;
            drl_end += holepos;
            GRFilledSegment( nullptr, aDC, drl_start, drl_end, seg_width, fillcolor );
            GRCSegm( nullptr, aDC, drl_start, drl_end, seg_width, hole_color );
        }
            break;

        default:
            break;
        }

        if( aDrawInfo.m_IsPrinting )
            GRForceBlackPen( blackpenstate );
    }

    // Draw "No connect" ( / or \ or cross X ) if necessary
    if( GetNetCode() == 0 && aDrawInfo.m_ShowNCMark )
    {
        int dx0 = std::min( halfsize.x, halfsize.y );

        if( m_layerMask[F_Cu] )    /* Draw \ */
            GRLine( nullptr, aDC, holepos.x - dx0, holepos.y - dx0,
                    holepos.x + dx0, holepos.y + dx0, 0, aDrawInfo.m_NoNetMarkColor );

        if( m_layerMask[B_Cu] )     // Draw /
            GRLine( nullptr, aDC, holepos.x + dx0, holepos.y - dx0,
                    holepos.x - dx0, holepos.y + dx0, 0, aDrawInfo.m_NoNetMarkColor );
    }

    // Draw the pad number
    if( !aDrawInfo.m_Display_padnum && !aDrawInfo.m_Display_netname )
        return;

    wxPoint  tpos0 = shape_pos;     // Position of the centre of text
    wxPoint  tpos  = tpos0;
    wxSize   AreaSize;              // size of text area, normalized to AreaSize.y < AreaSize.x
    wxString shortname;
    int      shortname_len = 0;

    if( aDrawInfo.m_Display_netname )
    {
        shortname = UnescapeString( GetShortNetname() );
        shortname_len = shortname.Len();
    }

    if( GetShape() == PAD_SHAPE_CIRCLE )
        angle = 0;

    AreaSize = m_Size;

    if( m_Size.y > m_Size.x )
    {
        angle += 900;
        AreaSize.x = m_Size.y;
        AreaSize.y = m_Size.x;
    }

    if( shortname_len > 0 )       // if there is a netname, provides room to display this netname
    {
        AreaSize.y /= 2;          // Text used only the upper area of the
                                  // pad. The lower area displays the net name
        tpos.y -= AreaSize.y / 2;
    }

    // Calculate the position of text, that is the middle point of the upper
    // area of the pad
    RotatePoint( &tpos, shape_pos, angle );

    // Draw text with an angle between -90 deg and + 90 deg
    double t_angle = angle;
    NORMALIZE_ANGLE_90( t_angle );

    /* Note: in next calculations, texte size is calculated for 3 or more
     * chars.  Of course, pads numbers and nets names can have less than 3
     * chars. but after some tries, i found this is gives the best look
     */
    constexpr int MIN_CHAR_COUNT = 3;

    unsigned int tsize;

    if( aDrawInfo.m_Display_padnum )
    {
        int numpad_len = std::max( (int) m_name.Length(), MIN_CHAR_COUNT );
        tsize = std::min( (int) AreaSize.y, AreaSize.x / numpad_len );

        if( aDC->LogicalToDeviceXRel( tsize ) >= MIN_TEXT_SIZE ) // Not drawable when size too small.
        {
            // tsize reserve room for marges and segments thickness
            tsize = ( tsize * 7 ) / 10;
            GRHaloText( aDC, tpos, aDrawInfo.m_Color, BLACK, WHITE, m_name, t_angle,
                        wxSize( tsize , tsize ), GR_TEXT_HJUSTIFY_CENTER, GR_TEXT_VJUSTIFY_CENTER,
                        tsize / 7, false, false );

        }
    }

    // display the short netname, if exists
    if( shortname_len == 0 )
        return;

    shortname_len = std::max( shortname_len, MIN_CHAR_COUNT );
    tsize = std::min( AreaSize.y, AreaSize.x / shortname_len );

    if( aDC->LogicalToDeviceXRel( tsize ) >= MIN_TEXT_SIZE )  // Not drawable in size too small.
    {
        tpos = tpos0;

        if( aDrawInfo.m_Display_padnum )
            tpos.y += AreaSize.y / 2;

        RotatePoint( &tpos, shape_pos, angle );

        // tsize reserve room for marges and segments thickness
        tsize = ( tsize * 7 ) / 10;
        GRHaloText( aDC, tpos, aDrawInfo.m_Color, BLACK, WHITE, shortname, t_angle,
                    wxSize( tsize, tsize ), GR_TEXT_HJUSTIFY_CENTER, GR_TEXT_VJUSTIFY_CENTER,
                    tsize / 7, false, false );
    }
}


/**
 * Function BuildSegmentFromOvalShape
 * Has meaning only for OVAL (and ROUND) pads.
 * Build an equivalent segment having the same shape as the OVAL shape,
 * aSegStart and aSegEnd are the ending points of the equivalent segment of the shape
 * aRotation is the asked rotation of the segment (usually m_Orient)
 */
int D_PAD::BuildSegmentFromOvalShape( wxPoint& aSegStart, wxPoint& aSegEnd, double aRotation,
                                      const wxSize& aMargin ) const
{
    int width;

    if( m_Size.y < m_Size.x )     // Build an horizontal equiv segment
    {
        int delta   = ( m_Size.x - m_Size.y ) / 2;
        aSegStart.x = -delta - aMargin.x;
        aSegStart.y = 0;
        aSegEnd.x = delta + aMargin.x;
        aSegEnd.y = 0;
        width = m_Size.y + ( aMargin.y * 2 );
    }
    else        // Vertical oval: build a vertical equiv segment
    {
        int delta   = ( m_Size.y -m_Size.x ) / 2;
        aSegStart.x = 0;
        aSegStart.y = -delta - aMargin.y;
        aSegEnd.x = 0;
        aSegEnd.y = delta + aMargin.y;
        width = m_Size.x + ( aMargin.x * 2 );
    }

    if( aRotation )
    {
        RotatePoint( &aSegStart, aRotation);
        RotatePoint( &aSegEnd, aRotation);
    }

    return width;
}


void D_PAD::BuildPadPolygon( wxPoint aCoord[4], wxSize aInflateValue,
                             double aRotation ) const
{
    wxSize delta;
    wxSize halfsize;

    halfsize.x = m_Size.x >> 1;
    halfsize.y = m_Size.y >> 1;

    switch( GetShape() )
    {
        case PAD_SHAPE_RECT:
            // For rectangular shapes, inflate is easy
            halfsize += aInflateValue;

            // Verify if do not deflate more than than size
            // Only possible for inflate negative values.
            if( halfsize.x < 0 )
                halfsize.x = 0;

            if( halfsize.y < 0 )
                halfsize.y = 0;
            break;

        case PAD_SHAPE_TRAPEZOID:
            // Trapezoidal pad: verify delta values
            delta.x = ( m_DeltaSize.x >> 1 );
            delta.y = ( m_DeltaSize.y >> 1 );

            // be sure delta values are not to large
            if( (delta.x < 0) && (delta.x <= -halfsize.y) )
                delta.x = -halfsize.y + 1;

            if( (delta.x > 0) && (delta.x >= halfsize.y) )
                delta.x = halfsize.y - 1;

            if( (delta.y < 0) && (delta.y <= -halfsize.x) )
                delta.y = -halfsize.x + 1;

            if( (delta.y > 0) && (delta.y >= halfsize.x) )
                delta.y = halfsize.x - 1;
        break;

        default:    // is used only for rect and trap. pads
            return;
    }

    // Build the basic rectangular or trapezoid shape
    // delta is null for rectangular shapes
    aCoord[0].x = -halfsize.x - delta.y;     // lower left
    aCoord[0].y = +halfsize.y + delta.x;

    aCoord[1].x = -halfsize.x + delta.y;     // upper left
    aCoord[1].y = -halfsize.y - delta.x;

    aCoord[2].x = +halfsize.x - delta.y;     // upper right
    aCoord[2].y = -halfsize.y + delta.x;

    aCoord[3].x = +halfsize.x + delta.y;     // lower right
    aCoord[3].y = +halfsize.y - delta.x;

    // Offsetting the trapezoid shape id needed
    // It is assumed delta.x or/and delta.y == 0
    if( GetShape() == PAD_SHAPE_TRAPEZOID && (aInflateValue.x != 0 || aInflateValue.y != 0) )
    {
        double angle;
        wxSize corr;

        if( delta.y )    // lower and upper segment is horizontal
        {
            // Calculate angle of left (or right) segment with vertical axis
            angle = atan2( (double) m_DeltaSize.y, (double) m_Size.y );

            // left and right sides are moved by aInflateValue.x in their perpendicular direction
            // We must calculate the corresponding displacement on the horizontal axis
            // that is delta.x +- corr.x depending on the corner
            corr.x  = KiROUND( tan( angle ) * aInflateValue.x );
            delta.x = KiROUND( aInflateValue.x / cos( angle ) );

            // Horizontal sides are moved up and down by aInflateValue.y
            delta.y = aInflateValue.y;

            // corr.y = 0 by the constructor
        }
        else if( delta.x )          // left and right segment is vertical
        {
            // Calculate angle of lower (or upper) segment with horizontal axis
            angle = atan2( (double) m_DeltaSize.x, (double) m_Size.x );

            // lower and upper sides are moved by aInflateValue.x in their perpendicular direction
            // We must calculate the corresponding displacement on the vertical axis
            // that is delta.y +- corr.y depending on the corner
            corr.y  = KiROUND( tan( angle ) * aInflateValue.y );
            delta.y = KiROUND( aInflateValue.y / cos( angle ) );

            // Vertical sides are moved left and right by aInflateValue.x
            delta.x = aInflateValue.x;

            // corr.x = 0 by the constructor
        }
        else                                    // the trapezoid is a rectangle
        {
            delta = aInflateValue;              // this pad is rectangular (delta null).
        }

        aCoord[0].x += -delta.x - corr.x;       // lower left
        aCoord[0].y += delta.y + corr.y;

        aCoord[1].x += -delta.x + corr.x;     // upper left
        aCoord[1].y += -delta.y - corr.y;

        aCoord[2].x += delta.x - corr.x;     // upper right
        aCoord[2].y += -delta.y + corr.y;

        aCoord[3].x += delta.x + corr.x;     // lower right
        aCoord[3].y += delta.y - corr.y;

        /* test coordinates and clamp them if the offset correction is too large:
         * Note: if a coordinate is bad, the other "symmetric" coordinate is bad
         * So when a bad coordinate is found, the 2 symmetric coordinates
         * are set to the minimun value (0)
         */

        if( aCoord[0].x > 0 )       // lower left x coordinate must be <= 0
            aCoord[0].x = aCoord[3].x = 0;

        if( aCoord[1].x > 0 )       // upper left x coordinate must be <= 0
            aCoord[1].x = aCoord[2].x = 0;

        if( aCoord[0].y < 0 )       // lower left y coordinate must be >= 0
            aCoord[0].y = aCoord[1].y = 0;

        if( aCoord[3].y < 0 )       // lower right y coordinate must be >= 0
            aCoord[3].y = aCoord[2].y = 0;
    }

    if( aRotation )
    {
        for( int ii = 0; ii < 4; ii++ )
            RotatePoint( &aCoord[ii], aRotation );
    }
}
