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

typedef std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> GeoPtr;

namespace encviz
{

/**
 * Mapping of S-57 standard COLOUR id's to css named colors
 */
const std::map<int, std::string> ENC_COLORS = {
    {0, "none"}, // useful default value, not in standard
    {1, "white"},
    {2, "black"},
    {3, "red"},
    {4, "green"},
    {5, "blue"},
    {6, "yellow"},
    {7, "grey"},
    {8, "brown"},
    {9, "darkorange"}, // amber
    {10, "violet"},
    {11, "orange"},
    {12, "magenta"},
    {13, "pink"}
};

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
enc_renderer::enc_renderer(const char *config_file)
{
    // Load specified, or default config path
    if (config_file != nullptr)
    {
        load_config(config_file);
    }
    else
    {
        // Default to ~/.encviz/config.xml
        fs::path default_path = getenv("HOME");
        default_path.append(".encviz/config.xml");
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
        double oversample = 0.2;
        double width = bbox.MaxX - bbox.MinX;
        double height = bbox.MaxY - bbox.MinY;
        bbox.MinX -= oversample * (width/2);
        bbox.MaxX += oversample * (width/2);
        bbox.MinY -= oversample * (height/2);
        bbox.MaxY += oversample * (height/2);
    }

    // Compute minimum presentation scale, based on average latitude and zoom
    // TODO - Is this the right computation?
    //double avgLat = (bbox.MinY + bbox.MaxY) / 2;
    //int scale_min = (int)round(min_scale0_ * cos(avgLat * M_PI / 180) / pow(2, z));

	// Hard-code presentation scales that look ok for now
	int scale_min = 1200000;
	if (z >= 7)
		scale_min = 675000;
	if (z >= 11)
		scale_min = 35000;
	if (z >= 13)
		scale_min = 4500;
	if (z >= 15)
		scale_min = 2200;
			
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

	std::cout << "Render Tile: " << std::endl;
	std::cout << " min scale: " << scale_min << std::endl;
    // Render style layers
    for (const auto &lstyle : style.layers)
    {
		if (lstyle.verbose)
			printf("  Layer: %s\n", lstyle.layer_name.c_str());

		// New geometry collection to compile all the polygons
		GeoPtr multi_poly(OGRGeometryFactory::createGeometry(wkbMultiPolygon), &OGRGeometryFactory::destroyGeometry);
	
        // Render feature geometry in this layer
        OGRLayer *tile_layer = tile_data->GetLayerByName(lstyle.layer_name.c_str());
        for (const auto &feat : tile_layer)
        {
            OGRGeometry *geo = feat->GetGeometryRef();
			
			// Render M_COVR
			if (std::string(feat->GetDefnRef()->GetName()) == "M_COVR")
			{
				OGRwkbGeometryType gtype = geo->getGeometryType();
				switch (gtype)
				{
				case wkbPolygon: // 6
					render_poly(cr, geo->toPolygon(), wm, lstyle);
					break;
				case wkbMultiPolygon: // 10
					for (const OGRPolygon *child : geo->toMultiPolygon())
					{
						render_poly(cr, child, wm, lstyle);
					}
					break;
				default:
					break;
				}
			}
			// Render DEPARE
			else if (std::string(feat->GetDefnRef()->GetName()) == "DEPARE"
					 || std::string(feat->GetDefnRef()->GetName()) == "DRGARE")
			{
				OGRwkbGeometryType gtype = geo->getGeometryType();
				switch (gtype)
				{
				case wkbPolygon: // 6
					render_depare(cr, geo->toPolygon(), wm, lstyle, feat.get());
					break;
				case wkbMultiPolygon: // 10
					for (const OGRPolygon *child : geo->toMultiPolygon())
					{
						render_depare(cr, child, wm, lstyle, feat.get());
					}
					break;
				default:
					break;
				}
			}
			else
			{
				// render basic geometries
				render_geo(cr, geo, wm, lstyle, multi_poly.get());
			}
			
			// Render anything with a buoy shape as a buoy
			int buoy_shape_idx = feat->GetFieldIndex("BOYSHP");
			if (buoy_shape_idx != -1)
			{
				render_buoy(cr, geo->toPoint(), wm, lstyle, feat.get());
			}

			// Render anything with a beacon shape as a beacon
			int beacon_shape_idx = feat->GetFieldIndex("BCNSHP");
			if (beacon_shape_idx != -1)
			{
				render_beacon(cr, geo->toPoint(), wm, lstyle, feat.get());
			}

			// Render Fog Signals
			if (std::string(feat->GetDefnRef()->GetName()) == "FOGSIG")
			{
				render_fog(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Lights
			else if (std::string(feat->GetDefnRef()->GetName()) == "LIGHTS")
			{
				render_light(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Landmark
			else if (std::string(feat->GetDefnRef()->GetName()) == "LNDMRK")
			{
				render_landmark(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Silo/Tank
			else if (std::string(feat->GetDefnRef()->GetName()) == "SILTNK")
			{
				render_silotank(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Rocks
			else if (std::string(feat->GetDefnRef()->GetName()) == "UWTROC")
			{
				render_rock(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Obstructions
			else if (std::string(feat->GetDefnRef()->GetName()) == "OBSTRN")
			{
				render_obstruction(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Wrecks
			else if (std::string(feat->GetDefnRef()->GetName()) == "WRECKS")
			{
				render_wreck(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render Anchor Berths
			else if (std::string(feat->GetDefnRef()->GetName()) == "ACHBRT")
			{
				render_anchor(cr, geo->toPoint(), wm, lstyle, feat.get());
			}
			// Render traffic separation scheme parts
			else if (std::string(feat->GetDefnRef()->GetName()) == "TSSLPT")
			{
				render_traffic_sep_part(cr, geo->toPolygon(), wm, lstyle, feat.get());
			}
			// Render name of a land area
			else if (std::string(feat->GetDefnRef()->GetName()) == "LNDARE")
			{
				render_named_area(cr, geo->toPolygon(), wm, lstyle, feat.get());
			}
			// Render name of a sea areas
			else if (std::string(feat->GetDefnRef()->GetName()) == "SEAARE")
			{
				render_named_area(cr, geo->toPolygon(), wm, lstyle, feat.get());
			}
			// Render name of a land regions
			else if (std::string(feat->GetDefnRef()->GetName()) == "LNDRGN")
			{
				render_named_area(cr, geo->toPolygon(), wm, lstyle, feat.get());
			}
			// Render name of a cities
			else if (std::string(feat->GetDefnRef()->GetName()) == "BUAARE")
			{
				render_named_area(cr, geo->toPolygon(), wm, lstyle, feat.get());
			}
			
        }

		// Render all polygons together after other features
		// because we have now built up the whole list and can
		// union them together.
		// Skip if DEPARE though because those are all touching
		// but need to be different colors
		if (lstyle.layer_name != "DEPARE"
			&& lstyle.layer_name != "DRGARE")
		{
			render_multipoly(cr, multi_poly.get(), wm, lstyle);
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
                              const web_mercator &wm, const layer_style &style,
							  OGRGeometry *late_render_polygons)
{
	if (style.verbose)
		std::cout << "Render GEO: " << geo->getGeometryName() << std::endl;
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
            render_depth(cr, geo->toPoint(), geo->toPoint()->getZ(), wm, style);
            break;

        case wkbMultiPoint25D: // 0x80000004
            // TODO - SOUNDG only?
            for (const OGRPoint *child : geo->toMultiPoint())
            {
                render_depth(cr, child, child->getZ(), wm, style);
            }
            break;

        case wkbLineString: // 2
            render_line(cr, geo->toLineString(), wm, style);
            break;

        case wkbMultiLineString: // 5
            for (const OGRGeometry *child : geo->toMultiLineString())
            {
                render_geo(cr, child, wm, style, late_render_polygons);
            }
            break;

        case wkbPolygon: // 6
            //render_poly(cr, geo->toPolygon(), wm, style);
			// Copy all polygons on this layer into one geometry object
			late_render_polygons->toMultiPolygon()->addGeometry(geo);
            break;

        case wkbMultiPolygon: // 10
            for (const OGRPolygon *child : geo->toMultiPolygon())
            {
                //render_poly(cr, child, wm, style);
				// Copy all polygons on this layer into one geometry object
				late_render_polygons->toMultiPolygon()->addGeometry(child);
            }
            break;

        case wkbGeometryCollection: // 7
            for (const OGRGeometry *child : geo->toGeometryCollection())
            {
                render_geo(cr, child, wm, style, late_render_polygons);
            }
            break;

        default:
            throw std::runtime_error("Unhandled geometry of type " +
                                     std::to_string(gtype));
    }
}

void enc_renderer::render_multipoly(cairo_t *cr, const OGRGeometry *geo,
									const web_mercator &wm, const layer_style &style)
{
	OGRwkbGeometryType gtype = geo->getGeometryType();
	if (gtype != wkbMultiPolygon)
	{
		return;
	}

	// Perform union of all polygons before drawing
	// TODO: would it be faster to just cheat in render poly and draw a 2px
	// border of the same color... probably
	GeoPtr union_polygons(geo->UnionCascaded(), &OGRGeometryFactory::destroyGeometry);
	gtype = union_polygons->getGeometryType();
	
	if (gtype == wkbPolygon)
	{
		render_poly(cr, union_polygons->toPolygon(), wm, style);
	}
	else if (gtype == wkbMultiPolygon)
	{
		for (const OGRPolygon *child : union_polygons->toMultiPolygon())
		{
			render_poly(cr, child, wm, style);
		}
	}
	return;
}

/**
 * Render Depth Value
 *
 * \param[out] cr Image context
 * \param[in] geo Feature geometry
 * \param[in] wm Web Mercator point mapper
 * \param[in] style Feature style
 */
void enc_renderer::render_depth(cairo_t *cr, const OGRPoint *geo, double depth,
                                const web_mercator &wm, const layer_style &style)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

    // TODO - Could do this better?
    int depth_m = std::floor(depth); // depth in m
    int depth_dm = std::floor((depth - depth_m) * 10); // depth remainder in decimeters
    char m_text[64] = {};
    snprintf(m_text, sizeof(m_text)-1, "%d", depth_m);
    char dm_text[64] = {};
    snprintf(dm_text, sizeof(dm_text)-1, "%d", depth_dm);

    // Set text style
    set_color(cr, style.line_color);
    cairo_select_font_face(cr, "monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    
    // Determine text render size
    cairo_text_extents_t m_extents = {};
    cairo_text_extents(cr, m_text, &m_extents);

    cairo_text_extents_t dm_extents = {};
    cairo_text_extents(cr, dm_text, &dm_extents);

    // Draw text
    if (depth_dm == 0)
    {
		// draw sounding text without a subscript
		cairo_move_to(cr, c.x - m_extents.width/2, c.y + m_extents.height/2);
		cairo_show_text(cr, m_text);
    }
    else
    {
		// draw sounding with a dm subscript
		float width = m_extents.width + dm_extents.width;
		float height = m_extents.height + dm_extents.height/2;
		// center of the whole text is c.x, c.y
		// top left of big text is x,y
		float x = c.x - width / 2;
		float y = c.y + height / 2;
		// big number
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, m_text);
		// subscript is to the right and half way down the big number
		cairo_move_to(cr, x + m_extents.width, y + m_extents.height / 2);
		cairo_show_text(cr, dm_text);
    }
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
	if (style.marker_shape == SQUARE_MARKER)
	{
		cairo_rectangle(cr,
						c.x - style.marker_size / 2,
						c.y - style.marker_size / 2,
						style.marker_size,
						style.marker_size);
	}
	else
	{
		cairo_arc(cr, c.x, c.y, style.marker_size, 0, 2 * M_PI);
	}

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
    if (geo->IsEmpty() || !geo->IsValid())
        return;
    //std::cout << "Render polygon: " << geo->exportToJson() << std::endl;
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

void enc_renderer::render_depare(cairo_t *cr, const OGRPolygon *geo,
								 const web_mercator &wm, const layer_style &style,
								 const OGRFeature *feat)
{
	float mindepth = feat->GetFieldAsDouble("DRVAL1");
	float maxdepth = feat->GetFieldAsDouble("DRVAL2");
	if (style.verbose)
	{
		std::cout << "min_depth: " << mindepth << std::endl;
		std::cout << "max_depth: " << maxdepth << std::endl;
	}

	// DRGARE seems to not always have a max depth??
	if (mindepth > maxdepth)
		maxdepth = mindepth;
	
	layer_style tweaked_style = style;
	if (maxdepth < 3)
	{
		tweaked_style.fill_color = style.depare_colors.foreshore;
		tweaked_style.line_color = style.depare_colors.foreshore;
	}
	else if (maxdepth < 5)
	{
		tweaked_style.fill_color = style.depare_colors.very_shallow;
		tweaked_style.line_color = style.depare_colors.very_shallow;
	}
	else if (maxdepth < 10)
	{
		tweaked_style.fill_color = style.depare_colors.medium_shallow;
		tweaked_style.line_color = style.depare_colors.medium_shallow;
	}
	else if (maxdepth < 25)
	{
		tweaked_style.fill_color = style.depare_colors.medium_deep;
		tweaked_style.line_color = style.depare_colors.medium_deep;
	}
	else
	{
		tweaked_style.fill_color = style.depare_colors.deep;
		tweaked_style.line_color = style.depare_colors.deep;
	}
	tweaked_style.line_width = 2; // overdraw by 1 px to hide gaps

	render_poly(cr, geo, wm, tweaked_style);
}

void enc_renderer::render_buoy(cairo_t *cr, const OGRPoint *geo,
							   const web_mercator &wm, const layer_style &style,
							   const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	// Get buoy colors
	// Get colors if this elemnt has a list of colors
	std::vector<std::string> colors_list;
	std::vector<int> colors_list_int;
	char** colors = feat->GetFieldAsStringList("COLOUR");
	if ( colors )
	{
		colors_list = std::vector<std::string>(colors, colors + CSLCount(colors));
		for (auto color : colors_list)
			colors_list_int.push_back(std::stoi(color));
	}

    // create style sheet to set buoy colors
    // Note right now this is blindly assigning colors to
    // named svg elements "buoy_color_n" that must exist
    // and not using COLPAT, which specifies the pattern
    // (horizontal stripes, vert, etc.) of the colors
    int i = 0;
    std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
    for (auto color_code : colors_list_int)
    {
		i++;
		try
		{
			ss << "#buoy_color_" << i << "{\n"
			   << "  fill: " << ENC_COLORS.at(color_code) << ";\n"
			   << "}\n";
		}
		catch (const std::out_of_range &e)
		{
			// bad color
		}
    }
    std::string stylesheet = ss.str();

	// Get buoy shape
	int buoy_shape = feat->GetFieldAsInteger("BOYSHP");

    fs::path svg = "";
	std::string svg_tag = "BOYSHP_" + std::to_string(buoy_shape);
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Buoy: " << svg_tag << " -> " << svg << "  size: " << style.icon_size << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);
}

void enc_renderer::render_beacon(cairo_t *cr, const OGRPoint *geo,
								 const web_mercator &wm, const layer_style &style,
								 const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	// Get buoy colors
	// Get colors if this elemnt has a list of colors
	std::vector<std::string> colors_list;
	std::vector<int> colors_list_int;
	char** colors = feat->GetFieldAsStringList("COLOUR");
	if ( colors )
	{
		colors_list = std::vector<std::string>(colors, colors + CSLCount(colors));
		for (auto color : colors_list)
			colors_list_int.push_back(std::stoi(color));
	}

    // create style sheet to set buoy colors
    // Note right now this is blindly assigning colors to
    // named svg elements "buoy_color_n" that must exist
    // and not using COLPAT, which specifies the pattern
    // (horizontal stripes, vert, etc.) of the colors
    int i = 0;
    std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
    for (auto color_code : colors_list_int)
    {
		i++;
		try
		{
			ss << "#beacon_color_" << i << "{\n"
			   << "  fill: " << ENC_COLORS.at(color_code) << ";\n"
			   << "}\n";
		}
		catch (const std::out_of_range &e)
		{
			// bad color
		}
    }
    std::string stylesheet = ss.str();

	// Get beacon shape
	int beacon_shape = feat->GetFieldAsInteger("BCNSHP");

    fs::path svg = "";
	std::string svg_tag = "BCNSHP_" + std::to_string(beacon_shape);
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Beacon: " << svg_tag << " -> " << svg << "  size: " << style.icon_size << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);
}

void enc_renderer::render_fog(cairo_t *cr, const OGRPoint *geo,
							  const web_mercator &wm, const layer_style &style,
							  const OGRFeature *feat)
{
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

    fs::path svg = "";
	std::string svg_tag = "FOGSIG";
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Fog Signal: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, 50, 50, stylesheet);
}

void enc_renderer::render_light(cairo_t *cr, const OGRPoint *geo,
								const web_mercator &wm, const layer_style &style,
								const OGRFeature *feat)
{
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	// Get light color
	// Get colors if this elemnt has a list of colors
	std::vector<std::string> colors_list;
	std::vector<int> colors_list_int;
	char** colors = feat->GetFieldAsStringList("COLOUR");
	if ( colors )
	{
		colors_list = std::vector<std::string>(colors, colors + CSLCount(colors));
		for (auto color : colors_list)
			colors_list_int.push_back(std::stoi(color));
	}

    // create style sheet to set light colors
    // Note right now this is blindly assigning colors to
    // named svg element "light_color" that must exist
    int i = 0;
    std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
    for (auto color_code : colors_list_int)
    {
		i++;
		try
		{
			ss << "#light_color" << "{\n"
			   << "  fill: " << ENC_COLORS.at(color_code) << ";\n"
			   << "}\n";
		}
		catch (const std::out_of_range &e)
		{
			// bad color
		}
    }
    std::string stylesheet = ss.str();

    fs::path svg = "";
	std::string svg_tag = "LIGHTS";
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Light: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, 50, 50, stylesheet);
}

void enc_renderer::render_landmark(cairo_t *cr, const OGRPoint *geo,
								   const web_mercator &wm, const layer_style &style,
								   const OGRFeature *feat)
{
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

	// Get list of landmark's categories
	char** categories = feat->GetFieldAsStringList("CATLMK");
	std::vector<int> categories_list_int;
	std::vector<std::string> categories_list;
	if ( categories )
	{
		categories_list = std::vector<std::string>(categories, categories + CSLCount(categories));
		for (auto cat : categories_list)
			categories_list_int.push_back(std::stoi(cat));
	}

	std::string type = "";
	for (auto cat : categories_list_int)
	{
		switch (cat)
		{
		case 3: // chimney
			type = "_chimney";
			break;
		case 6: // flare stack
			type = "_flarestack";
			break;
		case 7: // mast
			type = "_mast";
			break;
		case 17: // tower
			type = "_tower";
			break;
		case 18:
		case 19:
			type = "_windturbine";
			break;
		}
	}

    fs::path svg = "";
	std::string svg_tag = "LNDMRK" + type;
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Landmark: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);
}

void enc_renderer::render_silotank(cairo_t *cr, const OGRPoint *geo,
								   const web_mercator &wm, const layer_style &style,
								   const OGRFeature *feat)
{
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

    fs::path svg = "";
	std::string svg_tag = "SILTNK";
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Silo/Tank: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);
}

void enc_renderer::render_rock(cairo_t *cr, const OGRPoint *geo,
							   const web_mercator &wm, const layer_style &style,
							   const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	encviz::color depare_color = {0,0,0,0};
	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n"
	   << ".obstruction {\n"
	   << "  fill: " << depare_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

	int water_level = feat->GetFieldAsInteger("WATLEV");
	int exposition = feat->GetFieldAsInteger("EXPSOU");
	//int quality = feat->GetFieldAsInteger("QUASOU");
	float depth = feat->GetFieldAsDouble("VALSOU");

	std::string wl = "awash";
	if (water_level == 3) // always submerged
		wl = "submerged";

	if (exposition == 2) // Rock shallower than surrounding area
	{
		// Deeper or shallower than 20 m are displayed differently
		if (depth < 20.0)
			wl = "shoaler"; 
		else
			wl = "shoaler_deep";
	}

    fs::path svg = "";
	std::string svg_tag = "UWTROC_" + wl;
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Rock: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);

	// render text if shoaler
	if (exposition == 2) // Rock shallower than surrounding area
	{
		render_depth(cr, geo, depth, wm, style);
	}
}

void enc_renderer::render_obstruction(cairo_t *cr, const OGRPoint *geo,
									  const web_mercator &wm, const layer_style &style,
									  const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	//int category = feat->GetFieldAsInteger("CATOBS");
	int water_level = feat->GetFieldAsInteger("WATLEV");
	int exposition = feat->GetFieldAsInteger("EXPSOU");
	//int quality = feat->GetFieldAsInteger("QUASOU");
	float depth = feat->GetFieldAsDouble("VALSOU");

	encviz::color depare_color = style.depare_colors.foreshore;
	
	std::string wl = "awash";
	if (water_level == 3) // always submerged
	{
		wl = "submerged";
		depare_color = {0,0,0,0}; // transparent background when submerged
	}

	if (exposition == 2) // Rock shallower than surrounding area
	{
		// Deeper or shallower than 20 m are displayed differently
		if (depth < 20.0)
		{
			wl = "shoaler";
			depare_color = style.depare_colors.very_shallow;
		}
		else
		{
			wl = "shoaler_deep";
			depare_color = style.depare_colors.medium_shallow;
		}
	}

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n"
	   << ".obstruction {\n"
	   << "  fill: " << depare_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

    fs::path svg = "";
	std::string svg_tag = "OBSTRN_" + wl;
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Obstruction: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);

	// render text if shoaler
	if (exposition == 2) // Rock shallower than surrounding area
	{
		render_depth(cr, geo, depth, wm, style);
	}
}

void enc_renderer::render_wreck(cairo_t *cr, const OGRPoint *geo,
								const web_mercator &wm, const layer_style &style,
								const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	int category = feat->GetFieldAsInteger("CATWRK");

	encviz::color depare_color = {0,0,0,0};
	if (category == 2) // dangerous wreck, set background color
	{
		depare_color = style.depare_colors.very_shallow;
	}

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n"
	   << ".obstruction {\n"
	   << "  fill: " << depare_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

    fs::path svg = "";
	std::string svg_tag = "WRECKS_" + std::to_string(category);
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
		std::cout << "Render Wreck: " << svg_tag << " -> " << svg << std::endl;

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);
}

void enc_renderer::render_anchor(cairo_t *cr, const OGRPoint *geo,
								 const web_mercator &wm, const layer_style &style,
								 const OGRFeature *feat)
{
    // Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(*geo);

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

	int category = feat->GetFieldAsInteger("CATACH");
	float radius = feat->GetFieldAsDouble("RADIUS");

    fs::path svg = "";
	std::string svg_tag = "ACHBRT";
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
	{
		std::cout << "anchor category: " << category << std::endl;
		std::cout << "anchor radius: " << radius << std::endl;
		std::cout << "Render Anchor Berth: " << svg_tag << " -> " << svg << std::endl;
	}

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet);

	// Render anchor radius
	coord m = wm.point_to_meters(*geo);
	coord m1 = m; m1.x += radius; m1.y += radius;
	coord c1 = wm.meters_to_pixels(m1);
	float radius_pixels = (std::abs(c1.x - c.x) + std::abs(c1.y - c.y))/2;

	cairo_arc(cr, c.x, c.y, radius_pixels, 0, 2 * M_PI);
	set_color(cr, style.icon_color);
	cairo_set_dash(cr, nullptr, 0, 0);
    cairo_set_line_width(cr, style.line_width);
    cairo_stroke(cr);
}

void argle()
{

}

void enc_renderer::render_traffic_sep_part(cairo_t *cr, const OGRPolygon *geo,
										   const web_mercator &wm, const layer_style &style,
										   const OGRFeature *feat)
{
	// Get centroid of traffic lane
	OGRPoint centroid;
	geo->Centroid(&centroid);
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(centroid);

	std::stringstream ss;
	ss << ".icon {\n"
	   << "  fill: " << style.icon_color << ";\n"
	   << "}\n";
	std::string stylesheet = ss.str();

	float direction = feat->GetFieldAsDouble("ORIENT");

    fs::path svg = "";
	std::string svg_tag = "TSSLPT";
	if (style.icons.find(svg_tag) != style.icons.end())
	{
		svg = style.icons.at(svg_tag);
	}

	if (style.verbose)
	{
		std::cout << "traffic direction: " << direction << std::endl;
		std::cout << "Render Traffic Direction: " << svg_tag << " -> " << svg << std::endl;
	}

    svg_.render_svg(cr, svg, c, style.icon_size, style.icon_size, stylesheet, direction);

	return;
}

void enc_renderer::render_named_area(cairo_t *cr, const OGRPolygon *geo,
									 const web_mercator &wm, const layer_style &style,
									 const OGRFeature *feat)
{
	// Get centroid of area
	OGRPoint centroid;
	geo->Centroid(&centroid);
	// Convert lat/lon to pixel coordinates
    coord c = wm.point_to_pixels(centroid);

	std::string place_name = feat->GetFieldAsString("OBJNAM");

	if (place_name != "")
	{
		char name[place_name.length() + 1];
		strcpy(name, place_name.c_str());

		// Set text style
		set_color(cr, style.line_color);
		cairo_select_font_face(cr, "monospace",
							   CAIRO_FONT_SLANT_NORMAL,
							   CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(cr, 10);
    
		// Determine text render size
		cairo_text_extents_t name_extents = {};
		cairo_text_extents(cr, name, &name_extents);

		// Draw text
		cairo_move_to(cr, c.x - name_extents.width/2, c.y + name_extents.height/2);
		cairo_show_text(cr, name);
    }
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
void enc_renderer::load_config(const fs::path &config_file)
{
    printf("Using config file: %s ...\n", config_file.string().c_str());

    // Load XML document 
	fs::path config_path = config_file.parent_path();
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
    //svg_.set_svg_path(svg_path);

    // Load styles
    for (const fs::directory_entry &entry : fs::directory_iterator(style_path))
    {
        fs::path p = entry.path();
        if (p.extension() == ".xml")
        {
            styles_[p.stem().string()] = load_style(p.string(), svg_path);
        }
    }
}

}; // ~namespace encviz
