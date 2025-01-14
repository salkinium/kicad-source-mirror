/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2007-2014 Jean-Pierre Charras  jp.charras at wanadoo.fr
 * Copyright (C) 1992-2017 KiCad Developers, see change_log.txt for contributors.
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
 * @file export_to_pcbnew.cpp
 * @brief Export the layers to Pcbnew.
 */

#include <vector>

#include <fctsys.h>
#include <common.h>
#include <confirm.h>
#include <macros.h>
#include <trigo.h>
#include <gerbview.h>
#include <gerbview_frame.h>
#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <select_layers_to_pcb.h>
#include <build_version.h>
#include <wildcards_and_files_ext.h>


// Imported function
extern const wxString GetPCBDefaultLayerName( LAYER_NUM aLayerNumber );


/* A helper class to export a Gerber set of files to Pcbnew
 */
class GBR_TO_PCB_EXPORTER
{
private:
    GERBVIEW_FRAME*         m_gerbview_frame;   // the main gerber frame
    wxString                m_pcb_file_name;    // BOARD file to write to
    FILE*                   m_fp;               // the board file
    int                     m_pcbCopperLayersCount;
    std::vector<wxPoint>    m_vias_coordinates; // list of already generated vias,
                                                // used to export only once a via
                                                // having a given coordinate
public:
    GBR_TO_PCB_EXPORTER( GERBVIEW_FRAME* aFrame, const wxString& aFileName );
    ~GBR_TO_PCB_EXPORTER();

    /**
     * Function ExportPcb
     * saves a board from a set of Gerber images.
     */
    bool    ExportPcb( LAYER_NUM* aLayerLookUpTable, int aCopperLayers );

private:
    /**
     * write a non copper line or arc to the board file.
     * @param aGbrItem = the Gerber item (line, arc) to export
     * @param aLayer = the technical layer to use
     */
    void    export_non_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * write anot filled  polygon item to the board file.
     * @param aGbrItem = the Gerber item (line, arc) to export
     * @param aLayer = the technical layer to use
     */
    void    writePcbPolygonItem( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * write a zone item to the board file.
     * Currently: only experimental, for tests
     * @param aGbrItem = the Gerber item (line, arc) to export
     * @param aLayer = the technical layer to use
     */
    void    writePcbZoneItem( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * write a track or via) to the board file.
     * @param aGbrItem = the Gerber item (line, arc, flashed) to export
     * @param aLayer = the copper layer to use
     */
    void    export_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * Function export_flashed_copper_item
     * write a via to the board file (always uses a via through).
     * @param aGbrItem = the flashed Gerber item to export
     */
    void    export_flashed_copper_item( GERBER_DRAW_ITEM* aGbrItem );

    /**
     * Function export_segline_copper_item
     * write a track (not via) to the board file.
     * @param aGbrItem = the Gerber item (line only) to export
     * @param aLayer = the copper layer to use
     */
    void    export_segline_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * Function export_segarc_copper_item
     * write a set of tracks (arcs are approximated by track segments)
     * to the board file.
     * @param aGbrItem = the Gerber item (arc only) to export
     * @param aLayer = the copper layer to use
     */
    void    export_segarc_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer );

    /**
     * function writePcbLineItem
     * basic write function to write a DRAWSEGMENT item or a TRACK item
     * to the board file, from a non flashed item
     */
    void    writePcbLineItem( bool aIsArc, wxPoint& aStart, wxPoint& aEnd,
                              int aWidth, LAYER_NUM aLayer, double aAngle = 0 );

    /**
     * function writeCopperLineItem
     * basic write function to write a a TRACK item
     * to the board file, from a non flashed item
     */
    void    writeCopperLineItem( wxPoint& aStart, wxPoint& aEnd,
                                 int aWidth, LAYER_NUM aLayer );

    /**
     * function writePcbHeader
     * Write a very basic header to the board file
     */
    void    writePcbHeader( LAYER_NUM* aLayerLookUpTable );

    /** In Pcbnew files units are mm for coordinates.
     * So MapToPcbUnits converts internal gerbview to mm any pcbnew value
     * @param aValue is a coordinate value to convert in mm
     */
    double MapToPcbUnits( int aValue )
    {
        return aValue / IU_PER_MM;
    }
};


GBR_TO_PCB_EXPORTER::GBR_TO_PCB_EXPORTER( GERBVIEW_FRAME* aFrame, const wxString& aFileName )
{
    m_gerbview_frame    = aFrame;
    m_pcb_file_name     = aFileName;
    m_fp                = NULL;
    m_pcbCopperLayersCount = 2;
}


GBR_TO_PCB_EXPORTER::~GBR_TO_PCB_EXPORTER()
{
}


/* Export data in Pcbnew format
 * remember Pcbnew uses a Y reversed axis, so we must negate all Y coordinates
 */
void GERBVIEW_FRAME::ExportDataInPcbnewFormat( wxCommandEvent& event )
{
    int layercount = 0;

    GERBER_FILE_IMAGE_LIST* images = GetGerberLayout()->GetImagesList();

    // Count the Gerber layers which are actually currently used
    for( LAYER_NUM ii = 0; ii < (LAYER_NUM)images->ImagesMaxCount(); ++ii )
    {
        if( images->GetGbrImage( ii ) )
            layercount++;
    }

    if( layercount == 0 )
    {
        DisplayInfoMessage( this,
                            _( "None of the Gerber layers contain any data" ) );
        return;
    }

    wxString        fileName;
    wxString        path = m_mruPath;

    wxFileDialog    filedlg( this, _( "Board File Name" ),
                             path, fileName, PcbFileWildcard(),
                             wxFD_SAVE | wxFD_OVERWRITE_PROMPT );

    if( filedlg.ShowModal() == wxID_CANCEL )
        return;

    fileName = filedlg.GetPath();

    /* Install a dialog frame to choose the mapping
     * between gerber layers and Pcbnew layers
     */
    LAYERS_MAP_DIALOG* layerdlg = new LAYERS_MAP_DIALOG( this );
    int ok = layerdlg->ShowModal();
    layerdlg->Destroy();

    if( ok != wxID_OK )
        return;

    m_mruPath = wxFileName( fileName ).GetPath();

    GBR_TO_PCB_EXPORTER gbr_exporter( this, fileName );

    gbr_exporter.ExportPcb( layerdlg->GetLayersLookUpTable(), layerdlg->GetCopperLayersCount() );
}


bool GBR_TO_PCB_EXPORTER::ExportPcb( LAYER_NUM* aLayerLookUpTable, int aCopperLayers )
{
    LOCALE_IO   toggle;     // toggles on, then off, the C locale.

    m_fp = wxFopen( m_pcb_file_name, wxT( "wt" ) );

    if( m_fp == NULL )
    {
        wxString msg;
        msg.Printf( _( "Cannot create file \"%s\"" ), GetChars( m_pcb_file_name ) );
        DisplayError( m_gerbview_frame, msg );
        return false;
    }

    m_pcbCopperLayersCount = aCopperLayers;

    writePcbHeader( aLayerLookUpTable );

    // create an image of gerber data
    // First: non copper layers:
    const int pcbCopperLayerMax = 31;
    GERBER_FILE_IMAGE_LIST* images = m_gerbview_frame->GetGerberLayout()->GetImagesList();

    for( unsigned layer = 0; layer < images->ImagesMaxCount(); ++layer )
    {
        GERBER_FILE_IMAGE* gerber = images->GetGbrImage( layer );

        if( gerber == NULL )    // Graphic layer not yet used
            continue;

        LAYER_NUM pcb_layer_number = aLayerLookUpTable[layer];

        if( !IsPcbLayer( pcb_layer_number ) )
            continue;

        if( pcb_layer_number <= pcbCopperLayerMax ) // copper layer
            continue;

        GERBER_DRAW_ITEM* gerb_item = gerber->GetItemsList();

        for( ; gerb_item; gerb_item = gerb_item->Next() )
            export_non_copper_item( gerb_item, pcb_layer_number );
    }

    // Copper layers
    for( unsigned layer = 0; layer < images->ImagesMaxCount(); ++layer )
    {
        GERBER_FILE_IMAGE* gerber = images->GetGbrImage( layer );

        if( gerber == NULL )    // Graphic layer not yet used
            continue;

        LAYER_NUM pcb_layer_number = aLayerLookUpTable[layer];

        if( pcb_layer_number < 0 || pcb_layer_number > pcbCopperLayerMax )
            continue;

        GERBER_DRAW_ITEM* gerb_item = gerber->GetItemsList();

        for( ; gerb_item; gerb_item = gerb_item->Next() )
            export_copper_item( gerb_item, pcb_layer_number );
    }

    fprintf( m_fp, ")\n" );

    fclose( m_fp );
    m_fp = NULL;
    return true;
}


void GBR_TO_PCB_EXPORTER::export_non_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    bool isArc = false;

    double     angle   = 0;
    wxPoint seg_start   = aGbrItem->m_Start;
    wxPoint seg_end     = aGbrItem->m_End;

    if( aGbrItem->m_Shape == GBR_POLYGON )
    {
        writePcbPolygonItem( aGbrItem, aLayer );
        return;
    }

    if( aGbrItem->m_Shape == GBR_ARC )
    {
        double  a = atan2( (double) ( aGbrItem->m_Start.y - aGbrItem->m_ArcCentre.y),
                           (double) ( aGbrItem->m_Start.x - aGbrItem->m_ArcCentre.x ) );
        double  b = atan2( (double) ( aGbrItem->m_End.y - aGbrItem->m_ArcCentre.y ),
                           (double) ( aGbrItem->m_End.x - aGbrItem->m_ArcCentre.x ) );

        isArc       = true;
        angle       = RAD2DEG(b - a);
        seg_start   = aGbrItem->m_ArcCentre;

        // Ensure arc orientation is CCW
        if( angle < 0 )
            angle += 360.0;
    }

    // Reverse Y axis:
    seg_start.y = -seg_start.y;
    seg_end.y = -seg_end.y;
    writePcbLineItem( isArc, seg_start, seg_end, aGbrItem->m_Size.x, aLayer, angle );
}


void GBR_TO_PCB_EXPORTER::export_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    switch( aGbrItem->m_Shape )
    {
    case GBR_SPOT_CIRCLE:
    case GBR_SPOT_RECT:
    case GBR_SPOT_OVAL:
        // replace spots with vias when possible
        export_flashed_copper_item( aGbrItem );
        break;

    case GBR_ARC:
        export_segarc_copper_item( aGbrItem, aLayer );
        break;

    case GBR_POLYGON:
        // One can use a polygon or a zone to output a Gerber region.
        // none are perfect.
        // The current way is use a polygon, as the zone export
        // is exprimental and only for tests.
#if 1
        writePcbPolygonItem( aGbrItem, aLayer );
#else
        // Only for tests:
        writePcbZoneItem( aGbrItem, aLayer );
#endif
        break;

    default:
        export_segline_copper_item( aGbrItem, aLayer );
        break;
    }
}


void GBR_TO_PCB_EXPORTER::export_segline_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    wxPoint seg_start, seg_end;

    seg_start   = aGbrItem->m_Start;
    seg_end     = aGbrItem->m_End;

    // Reverse Y axis:
    seg_start.y = -seg_start.y;
    seg_end.y = -seg_end.y;

    writeCopperLineItem( seg_start, seg_end, aGbrItem->m_Size.x, aLayer );
}


void GBR_TO_PCB_EXPORTER::writeCopperLineItem( wxPoint& aStart, wxPoint& aEnd,
                                               int aWidth, LAYER_NUM aLayer )
{
  fprintf( m_fp, "(segment (start %s %s) (end %s %s) (width %s) (layer %s) (net 0))\n",
                  Double2Str( MapToPcbUnits(aStart.x) ).c_str(),
                  Double2Str( MapToPcbUnits(aStart.y) ).c_str(),
                  Double2Str( MapToPcbUnits(aEnd.x) ).c_str(),
                  Double2Str( MapToPcbUnits(aEnd.y) ).c_str(),
                  Double2Str( MapToPcbUnits( aWidth ) ).c_str(),
                  TO_UTF8( GetPCBDefaultLayerName( aLayer ) ) );
}


void GBR_TO_PCB_EXPORTER::export_segarc_copper_item( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    double  a = atan2( (double) ( aGbrItem->m_Start.y - aGbrItem->m_ArcCentre.y ),
                       (double) ( aGbrItem->m_Start.x - aGbrItem->m_ArcCentre.x ) );
    double  b = atan2( (double) ( aGbrItem->m_End.y - aGbrItem->m_ArcCentre.y ),
                       (double) ( aGbrItem->m_End.x - aGbrItem->m_ArcCentre.x ) );

    wxPoint start   = aGbrItem->m_Start;
    wxPoint end     = aGbrItem->m_End;

    /* Because Pcbnew does not know arcs in tracks,
     * approximate arc by segments (SEG_COUNT__CIRCLE segment per 360 deg)
     * The arc is drawn anticlockwise from the start point to the end point.
     */
    #define SEG_COUNT_CIRCLE    16
    #define DELTA_ANGLE         2 * M_PI / SEG_COUNT_CIRCLE

    // calculate the number of segments from a to b.
    // we want CNT_PER_360 segments fo a circle
    if( a > b )
        b += 2 * M_PI;

    wxPoint curr_start = start;
    wxPoint seg_start, seg_end;

    int     ii = 1;

    for( double rot = a; rot < (b - DELTA_ANGLE); rot += DELTA_ANGLE, ii++ )
    {
        seg_start = curr_start;
        wxPoint curr_end = start;
        RotatePoint( &curr_end, aGbrItem->m_ArcCentre,
                     -RAD2DECIDEG( DELTA_ANGLE * ii ) );
        seg_end = curr_end;
        // Reverse Y axis:
        seg_start.y = -seg_start.y;
        seg_end.y = -seg_end.y;
        writeCopperLineItem( seg_start, seg_end, aGbrItem->m_Size.x, aLayer );
        curr_start = curr_end;
    }

    if( end != curr_start )
    {
        seg_start   = curr_start;
        seg_end     = end;
        // Reverse Y axis:
        seg_start.y = -seg_start.y;
        seg_end.y = -seg_end.y;
        writeCopperLineItem( seg_start, seg_end, aGbrItem->m_Size.x, aLayer );
    }
}


/*
 * creates a via from a flashed gerber item.
 * Flashed items are usually pads or vias, so we try to export all of them
 * using vias
 */
void GBR_TO_PCB_EXPORTER::export_flashed_copper_item( GERBER_DRAW_ITEM* aGbrItem )
{
    // First, explore already created vias, before creating a new via
    for( unsigned ii = 0; ii < m_vias_coordinates.size(); ii++ )
    {
        if( m_vias_coordinates[ii] == aGbrItem->m_Start ) // Already created
            return;
    }

    m_vias_coordinates.push_back( aGbrItem->m_Start );

    wxPoint via_pos = aGbrItem->m_Start;
    int width   = (aGbrItem->m_Size.x + aGbrItem->m_Size.y) / 2;
    // Reverse Y axis:
    via_pos.y = -via_pos.y;

    // Layers are Front to Back
    fprintf( m_fp, " (via (at %s %s) (size %s)",
                  Double2Str( MapToPcbUnits(via_pos.x) ).c_str(),
                  Double2Str( MapToPcbUnits(via_pos.y) ).c_str(),
                  Double2Str( MapToPcbUnits( width ) ).c_str() );

    fprintf( m_fp, " (layers %s %s))\n",
                  TO_UTF8( GetPCBDefaultLayerName( F_Cu ) ),
                  TO_UTF8( GetPCBDefaultLayerName( B_Cu ) ) );
}

void GBR_TO_PCB_EXPORTER::writePcbHeader( LAYER_NUM* aLayerLookUpTable )
{
    fprintf( m_fp, "(kicad_pcb (version 4) (host Gerbview \"%s\")\n\n",
             TO_UTF8( GetBuildVersion() ) );

    // Write layers section
    fprintf( m_fp, "  (layers \n" );

    for( int ii = 0; ii < m_pcbCopperLayersCount; ii++ )
    {
        int id = ii;

        if( ii == m_pcbCopperLayersCount-1)
            id = B_Cu;

        fprintf( m_fp, "    (%d %s signal)\n", id, TO_UTF8( GetPCBDefaultLayerName( id ) ) );
    }

    for( int ii = B_Adhes; ii < PCB_LAYER_ID_COUNT; ii++ )
    {
        if( GetPCBDefaultLayerName( ii ).IsEmpty() )    // Layer not available for export
            continue;

        fprintf( m_fp, "    (%d %s user)\n", ii, TO_UTF8( GetPCBDefaultLayerName( ii ) ) );
    }

    fprintf( m_fp, "  )\n\n" );
}


void GBR_TO_PCB_EXPORTER::writePcbLineItem( bool aIsArc, wxPoint& aStart, wxPoint& aEnd,
                                            int aWidth, LAYER_NUM aLayer, double aAngle )
{
    if( aIsArc && ( aAngle == 360.0 ||  aAngle == 0 ) )
    {
        fprintf( m_fp, "(gr_circle (center %s %s) (end %s %s)(layer %s) (width %s))\n",
                 Double2Str( MapToPcbUnits(aStart.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aStart.y) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.y) ).c_str(),
                 TO_UTF8( GetPCBDefaultLayerName( aLayer ) ),
                 Double2Str( MapToPcbUnits( aWidth ) ).c_str()
                 );
    }
    else if( aIsArc )
    {
        fprintf( m_fp, "(gr_arc (start %s %s) (end %s %s) (angle %s)(layer %s) (width %s))\n",
                 Double2Str( MapToPcbUnits(aStart.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aStart.y) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.y) ).c_str(),
                 Double2Str( aAngle ).c_str(),
                 TO_UTF8( GetPCBDefaultLayerName( aLayer ) ),
                 Double2Str( MapToPcbUnits( aWidth ) ).c_str()
                 );
    }
    else
    {
        fprintf( m_fp, "(gr_line (start %s %s) (end %s %s)(layer %s) (width %s))\n",
                 Double2Str( MapToPcbUnits(aStart.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aStart.y) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.x) ).c_str(),
                 Double2Str( MapToPcbUnits(aEnd.y) ).c_str(),
                 TO_UTF8( GetPCBDefaultLayerName( aLayer ) ),
                 Double2Str( MapToPcbUnits( aWidth ) ).c_str()
                 );
    }
}


void GBR_TO_PCB_EXPORTER::writePcbPolygonItem( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    SHAPE_POLY_SET polys = aGbrItem->m_Polygon;

    // Cleanup the polygon
    polys.Simplify( SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );

    // Ensure the polygon is valid:
    if( polys.OutlineCount() == 0 )
        return;

    polys.Fracture( SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );

    SHAPE_LINE_CHAIN& poly = polys.Outline( 0 );

    fprintf( m_fp, "(gr_poly (pts " );

    #define MAX_COORD_CNT 4
    int jj = MAX_COORD_CNT;
    int cnt_max = poly.PointCount() -1;

    // Do not generate last corner, if it is the same point as the first point:
    if( poly.CPoint( 0 ) == poly.CPoint( cnt_max ) )
        cnt_max--;

    for( int ii = 0; ii <= cnt_max; ii++ )
    {
        if( --jj == 0 )
        {
            jj = MAX_COORD_CNT;
            fprintf( m_fp, "\n" );
        }

        fprintf( m_fp, " (xy %s %s)", Double2Str( MapToPcbUnits( poly.CPoint( ii ).x ) ).c_str(),
                Double2Str( MapToPcbUnits( -poly.CPoint( ii ).y ) ).c_str() );
    }

    fprintf( m_fp, ")" );

    if( jj != MAX_COORD_CNT )
        fprintf( m_fp, "\n" );

    fprintf( m_fp, "(layer %s) (width 0) )\n",
             TO_UTF8( GetPCBDefaultLayerName( aLayer ) ) );
}


void GBR_TO_PCB_EXPORTER::writePcbZoneItem( GERBER_DRAW_ITEM* aGbrItem, LAYER_NUM aLayer )
{
    SHAPE_POLY_SET polys = aGbrItem->m_Polygon;
    polys.Simplify( SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );

    if( polys.OutlineCount() == 0 )
        return;

    fprintf( m_fp, "(zone (net 0) (net_name \"\") (layer %s) (tstamp 0000000) (hatch edge 0.508)\n",
            TO_UTF8( GetPCBDefaultLayerName( aLayer ) ) );

    fprintf( m_fp, "  (connect_pads (clearance 0.0))\n" );

    fprintf( m_fp, "  (min_thickness 0.1) (filled_areas_thickness no)\n"
                   "  (fill (thermal_gap 0.3) (thermal_bridge_width 0.3))\n" );

    // Now, write the zone outlines with holes.
    // first polygon is the main outline, next are holes
    // One cannot know the initial zone outline.
    // However most of (if not all) holes are just items with clearance,
    // not really a hole in the initial zone outline.
    // So we build a zone outline only with no hole.
    fprintf( m_fp, "  (polygon\n    (pts" );

    SHAPE_LINE_CHAIN& poly = polys.Outline( 0 );

    #define MAX_COORD_CNT 4
    int jj = MAX_COORD_CNT;
    int cnt_max = poly.PointCount() -1;

    // Do not generate last corner, if it is the same point as the first point:
    if( poly.CPoint( 0 ) == poly.CPoint( cnt_max ) )
        cnt_max--;

    for( int ii = 0; ii <= cnt_max; ii++ )
    {
        if( --jj == 0 )
        {
            jj = MAX_COORD_CNT;
            fprintf( m_fp, "\n   " );
        }

        fprintf( m_fp, " (xy %s %s)", Double2Str( MapToPcbUnits( poly.CPoint( ii ).x ) ).c_str(),
                 Double2Str( MapToPcbUnits( -poly.CPoint( ii ).y ) ).c_str() );
    }

    fprintf( m_fp, ")\n" );

    fprintf( m_fp, "  )\n)\n" );
}
