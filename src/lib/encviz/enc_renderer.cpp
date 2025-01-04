/**
 * \file
 * \brief ENC Renderer
 *
 * C++ abstraction class to handle visualization of ENC(S-57) chart data.
 */

#include <encviz/enc_renderer.h>
#include <encviz/xml_config.h>
#include <librsvg/rsvg.h>
#include <iostream>
namespace fs = std::filesystem;

namespace encviz
{

/**
 * Cairo Stream Callback
 *
 * \param[out] closure Pointer to std::vector<uint8_t> output stream
 * \param[in] data Data buffer
 * \param[in] length Length of data buffer
 */
static cairo_status_t cairo_write_to_vector(void *closure,
                                            const unsigned char *data,
                                            unsigned int length)
{
    // TODO - Can probably make this better
    std::vector<uint8_t> *output = (std::vector<uint8_t>*)closure;
    output->reserve(output->size() + length);
    for (unsigned int i = 0; i < length; i++)
    {
        output->push_back(data[i]);
    }
    return CAIRO_STATUS_SUCCESS;
}

/**
 * Constructor
 *
 * \param[in] tile_size Dimension of output image
 * \param[in] min_scale0 Min display scale at zoom=0
 */
enc_renderer::enc_renderer(const char *config_path)
{
    // Load specified, or default config path
    if (config_path != nullptr)
    {
        load_config(config_path);
    }
    else
    {
        // Default to ~/.encviz
        fs::path default_path = getenv("HOME");
        default_path.append(".encviz");
        load_config(default_path);
    }
}

/**
 * Render Chart Data
 *
 * \param[out] data PNG bytestream
 * \param[in] tc Tile coordinate system (WMTS or XYZ)
 * \param[in] x Tile X coordinate (horizontal)
 * \param[in] y Tile Y coordinate (vertical)
 * \param[in] z Tile Z coordinate (zoom)
 * \param[in] style Tile styling data
 * \return False if no data to render
 */
bool enc_renderer::render(std::vector<uint8_t> &data, tile_coords tc,
                          int x, int y, int z, const char *style_name)
{
    // Grab the style we need
    if (styles_.find(style_name) == styles_.end())
    {
        return false;
    }
    const render_style &style = styles_[style_name];

    // Collect the layers we need
    std::vector<std::string> layers;
    for (const layer_style &lstyle : style.layers)
    {
        layers.push_back(lstyle.layer_name);
    }

    // Get base tile boundaries
    encviz::web_mercator wm(x, y, z, tc, tile_size_);
    OGREnvelope bbox = wm.get_bbox_deg();

    // Oversample a bit so not clip text between tiles
    {
        double oversample = 0.1;
        double width = bbox.MaxX - bbox.MinX;
        double height = bbox.MaxY - bbox.MinY;
        bbox.MinX -= oversample * (width/2);
        bbox.MaxX += oversample * (width/2);
        bbox.MinY -= oversample * (height/2);
        bbox.MaxY += oversample * (height/2);
    }

    // Compute minimum presentation scale, based on average latitude and zoom
    // TODO - Is this the right computation?
    double avgLat = (bbox.MinY + bbox.MaxY) / 2;
    int scale_min = (int)round(min_scale0_ * cos(avgLat * M_PI / 180) / pow(2, z));

    // Export all data in this tile
    GDALDataset *tile_data = GetGDALDriverManager()->GetDriverByName("Memory")->
        Create("", 0, 0, 0, GDT_Unknown, nullptr);
    if (!enc_.export_data(tile_data, layers, bbox, scale_min))
    {
	printf("Error exporting tile data\n");
        return false;
    }

    // Create a cairo surface
    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, tile_size_, tile_size_);
    cairo_t *cr = cairo_create(surface);
                                                          
    // Flood background w/ white 0xffffff
    if (style.background.has_value())
    {
        set_color(cr, style.background.value());
        cairo_paint(cr);
    }

    printf("Render Tile:\n");
    // Render style layers
    for (const auto &lstyle : style.layers)
    {
	printf("  Layer: %s\n", lstyle.layer_name.c_str());
        // Render feature geometry in this layer
        OGRLayer *tile_layer = tile_data->GetLayerByName(lstyle.layer_name.c_str());
        for (const auto &feat : tile_layer)
        {
            OGRGeometry *geo = feat->GetGeometryRef();
            render_geo(cr, geo, wm, lstyle);

	    // Handle buoy icon rendering
	    int buoy_shape_idx = feat->GetFieldIndex("BOYSHP");
	    if (buoy_shape_idx != -1)
	    {
		// Get buoy shape
		int buoy_shape = feat->GetFieldAsInteger("BOYSHP");

		// Get buoy colors
		std::vector<std::string> colors_list;
		std::vector<int> colors_list_int;
		char** colors = feat->GetFieldAsStringList("COLOUR");
		if ( colors )
		{
		    colors_list = std::vector<std::string>(colors, colors + CSLCount(colors));
		    for (auto color : colors_list)
			colors_list_int.push_back(std::stoi(color));
		}

		std::cout << "Rendering buoy: " << feat->GetFieldAsString("OBJNAM") << std::endl;
		render_buoy(cr, geo->toPoint(), wm, lstyle, buoy_shape, colors_list_int);
	    }

	    /*
	    if (std::string(feat->GetDefnRef()->GetName()) == "BOYLAT")
	    {
		// Loop over all fields in this feature and print their names
		for (const auto &field : feat->GetDefnRef()->GetFields())
		{
		    printf("    Field: %s type: %d\n", field->GetNameRef(), field->GetType());
		}

		//printf(" %s : %s \n", "OBJNAM", feat->GetFieldAsString("OBJNAM"));
		//printf(" %s : %s \n", "BOYSHP", feat->GetFieldAsString("BOYSHP"));
	    }
	    */

	    
	    //printf("    Feature: %s : %d\n", feat->GetDefnRef()->GetName(), feat->GetDefnRef()->GetFieldCount());

	    //printf("    Field: %s : %d\n", feat->GetDefnRef()->GetFieldDefn(), feat->GetDefnRef()->GetFieldCount());
        }
    }

    // Write out image
    data.clear();
    cairo_status_t rc =
        cairo_surface_write_to_png_stream(surface,
                                          cairo_write_to_vector,
                                          &data);
    if (rc)
    {
        printf("Cairo write error %d : %s\n", rc,
               cairo_status_to_string(rc));
    }

    printf("Png Size: %lu\n", data.size());

    // Cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    GDALClose(tile_data);

    return true;
}

/**
 * Render Feature Geometry
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_geo(cairo_t *cr, const OGRGeometry *geo,
                              const web_mercator &wm, const layer_style &style)
{
    // What sort of geometry were we passed?
    OGRwkbGeometryType gtype = geo->getGeometryType();
    switch (gtype)
    {
        case wkbPoint: // 1
            render_point(cr, geo->toPoint(), wm, style);
            break;

        case wkbMultiPoint: // 4
            for (const OGRPoint *child : geo->toMultiPoint())
            {
                render_point(cr, child, wm, style);
            }
            break;

        case wkbPoint25D: // 0x80000001
            // TODO - SOUNDG only?
            render_depth(cr, geo->toPoint(), wm, style);
            break;

        case wkbMultiPoint25D: // 0x80000004
            // TODO - SOUNDG only?
            for (const OGRPoint *child : geo->toMultiPoint())
            {
                render_depth(cr, child, wm, style);
            }
            break;

        case wkbLineString: // 2
            render_line(cr, geo->toLineString(), wm, style);
            break;

        case wkbMultiLineString: // 5
            for (const OGRGeometry *child : geo->toMultiLineString())
            {
                render_geo(cr, child, wm, style);
            }
            break;

        case wkbPolygon: // 6
            render_poly(cr, geo->toPolygon(), wm, style);
            break;

        case wkbMultiPolygon: // 10
            for (const OGRPolygon *child : geo->toMultiPolygon())
            {
                render_poly(cr, child, wm, style);
            }
            break;

        case wkbGeometryCollection: // 7
            for (const OGRGeometry *child : geo->toGeometryCollection())
            {
                render_geo(cr, child, wm, style);
            }
            break;

        default:
            throw std::runtime_error("Unhandled geometry of type " +
                                     std::to_string(gtype));
    }
}

/**
 * Render Depth Value
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_depth(cairo_t *cr, const OGRPoint *geo,
                                const web_mercator &wm, const layer_style &style)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

    // TODO - Could do this better?
    char text[64] = {};
    snprintf(text, sizeof(text)-1, "%.1f", geo->getZ());

    // Determine text render size
    cairo_text_extents_t extents = {};
    cairo_text_extents(cr, text, &extents);

    // Draw text
    set_color(cr, style.line_color);
    cairo_select_font_face(cr, "monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, c.x - extents.width/2, c.y - extents.height/2);
    cairo_show_text(cr, text);
}

/**
 * Render Point Geometry
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_point(cairo_t *cr, const OGRPoint *geo,
                                const web_mercator &wm, const layer_style &style)
{
    // Skip render if not appropriate
    if (style.marker_size == 0)
    {
        return;
    }

    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

    // Draw circle
    cairo_arc(cr, c.x, c.y, style.marker_size, 0, 2 * M_PI);

    // Draw line and fill
    set_color(cr, style.fill_color);
    cairo_fill_preserve(cr);
    set_color(cr, style.line_color);
    cairo_set_line_width(cr, style.line_width);
    cairo_stroke(cr);
}

/**
 * Render LineString Geometry
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_line(cairo_t *cr, const OGRLineString *geo,
                               const web_mercator &wm, const layer_style &style)
{
    // Pass OGR points to cairo
    bool first = true;
    for (auto &point : geo)
    {
        // Convert lat/lon to pixel coordinates
        coord c = wm.point_to_pixels(point);

        // Mark first point as pen-down
        if (first)
        {
            cairo_move_to(cr, c.x, c.y);
            first = false;
        }
        else
        {
            cairo_line_to(cr, c.x, c.y);
        }
    }

    // Draw line
    set_color(cr, style.line_color);
    cairo_set_line_width(cr, style.line_width);
    switch (style.line_dash)
    {
	double dash;
    case 0:
	cairo_set_dash(cr, nullptr, 0, 0);
	break;
    case 1:
	dash = style.line_width;
	cairo_set_dash(cr, &dash, 1, 0);
	break;
    case 2:
	dash = style.line_width * 2;
	cairo_set_dash(cr, &dash, 1, 0);
	break;
    case 3:
	dash = style.line_width * 10;
	cairo_set_dash(cr, &dash, 1, 0);
	break;	
    }
    cairo_stroke(cr);
}

/**
 * Render Polygon Geometry
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_poly(cairo_t *cr, const OGRPolygon *geo,
                               const web_mercator &wm, const layer_style &style)
{
    // FIXME - Throw a fit if we see interior rings (not handled)
    if (geo->getNumInteriorRings() != 0)
    {
        //throw std::runtime_error("Unhandled polygon with interior rings");
    }

    // Pass OGR points to cairo
    bool first = true;
    for (auto &point : geo->getExteriorRing())
    {
        // Convert lat/lon to pixel coordinates
        coord c = wm.point_to_pixels(point);

        // Mark first point as pen-down
        if (first)
        {
            cairo_move_to(cr, c.x, c.y);
            first = false;
        }
        else
        {
            cairo_line_to(cr, c.x, c.y);
        }
    }

    // Draw line and fill
    set_color(cr, style.fill_color);
    cairo_fill_preserve(cr);
    set_color(cr, style.line_color);
    cairo_set_line_width(cr, style.line_width);
    switch (style.line_dash)
    {
	double dash;
    case 0:
	cairo_set_dash(cr, nullptr, 0, 0);
	break;
    case 1:
	dash = style.line_width;
	cairo_set_dash(cr, &dash, 1, 0);
	break;
    case 2:
	dash = style.line_width * 2;
	cairo_set_dash(cr, &dash, 1, 0);
	break;
    case 3:
	dash = style.line_width * 10;
	cairo_set_dash(cr, &dash, 1, 0);
	break;
    }
    cairo_stroke(cr);
}

void enc_renderer::render_buoy(cairo_t *cr, const OGRPoint *geo,
			       const web_mercator &wm, const layer_style &style,
			       const int buoy_shape,
			       std::vector<int> buoy_colors)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

    fs::path buoy_svg = "";
    switch (buoy_shape)
    {
    case 1: // conical
      buoy_svg = "Q_buoys_beacons/conical.svg";
      break;
    case 2: // can/cylindrical
      buoy_svg = "Q_buoys_beacons/can.svg";
      break;
    case 3: // spherical
      buoy_svg = "Q_buoys_beacons/spherical.svg";
      break;
    case 4: // pillar
      buoy_svg = "Q_buoys_beacons/pillar.svg";
      break;
    case 5: // spar
      buoy_svg = "Q_buoys_beacons/spar.svg";
      break;
    case 6: // barrel
      buoy_svg = "Q_buoys_beacons/barrel.svg";
      break;
    case 7: // super buoy
      buoy_svg = "Q_buoys_beacons/super_buoy.svg";
      break;
    }

    svg_.render_svg(cr, buoy_svg, c, 50, 50);
}

/**
 * Set Render Color
 *
 * \param[out] cr Image context
 * \param[in] c RGB color
 */
void enc_renderer::set_color(cairo_t *cr, const color &c)
{
    cairo_set_source_rgba(cr,
                          float(c.red) / 0xff,
                          float(c.green) / 0xff,
                          float(c.blue) / 0xff,
                          float(c.alpha) / 0xff);
}

/**
 * Load Configuration
 *
 * \param[in] config_path
 */
void enc_renderer::load_config(const fs::path &config_path)
{
    printf("Using config directory: %s ...\n", config_path.string().c_str());

    // Load XML document 
    fs::path config_file = config_path / "config.xml";
    printf(" - Reading %s ...\n", config_file.string().c_str());
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(config_file.string().c_str()))
    {
        // Parse error?
        throw std::runtime_error("Cannot parse " + config_file.string());
    }

    // Read in config
    tinyxml2::XMLElement *root = doc.RootElement();
    fs::path chart_path = xml_text(xml_query(root, "chart_path"));
    fs::path meta_path = xml_text(xml_query(root, "meta_path"));
    fs::path style_path = xml_text(xml_query(root, "style_path"));
    fs::path svg_path = xml_text(xml_query(root, "svg_path"));
    tile_size_ = atoi(xml_text(xml_query(root, "tile_size")));
    min_scale0_ = atof(xml_text(xml_query(root, "scale_base")));

    // Ensure paths are absolute
    if (chart_path.is_relative())
        chart_path = config_path / chart_path;
    if (meta_path.is_relative())
        meta_path = config_path / meta_path;
    if (style_path.is_relative())
        style_path = config_path / style_path;
    if (svg_path.is_relative())
        svg_path = config_path / svg_path;

    printf(" - Charts: %s\n", chart_path.string().c_str());
    printf(" - Metadata: %s\n", meta_path.string().c_str());
    printf(" - Styles: %s\n", style_path.string().c_str());
    printf(" - SVGs: %s\n", svg_path.string().c_str());
    printf(" - Tile Size: %d\n", tile_size_);
    printf(" - Scale Base: %g\n", min_scale0_);

    // Load charts
    enc_.set_cache_path(meta_path);
    enc_.load_charts(chart_path);

    // Set up svg load path
    svg_.set_svg_path(svg_path);

    // Load styles
    for (const fs::directory_entry &entry : fs::directory_iterator(style_path))
    {
        fs::path p = entry.path();
        if (p.extension() == ".xml")
        {
            styles_[p.stem().string()] = load_style(p.string());
        }
    }
}

}; // ~namespace encviz
