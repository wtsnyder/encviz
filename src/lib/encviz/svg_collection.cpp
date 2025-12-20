/**
 * \file
 * \brief SVG Renderer
 *
 * C++ abstraction class to handle finding and rendering svg images
 */

#include <encviz/svg_collection.h>
#include <librsvg/rsvg.h>
#include <iostream>
namespace fs = std::filesystem;

// Define a custom deleter for GObjects
struct GObjectDeleter {
    void operator()(RsvgHandle* handle) const {
        if (handle) {
            g_object_unref(handle);
        }
    }
	void operator()(GFile* handle) const {
        if (handle) {
            g_object_unref(handle);
        }
    }
};

typedef std::unique_ptr<RsvgHandle, GObjectDeleter> RsvgHandlePtr;
typedef std::unique_ptr<GFile, GObjectDeleter> GFilePtr;

namespace encviz
{

svg_collection::svg_collection()
	: svg_root_path_("")
{

}

void svg_collection::set_svg_path(const std::filesystem::path &svg_path)
{
    svg_root_path_ = svg_path;
}

bool svg_collection::render_svg(cairo_t *cr, std::filesystem::path &svg_path,
								coord center, double width, double height,
								std::string stylesheet, double rotation)
{
    fs::path full_path = svg_root_path_;
    full_path /= svg_path;
    
    GError *error = NULL;
    GFilePtr file(g_file_new_for_path(full_path.string().c_str()));

    //RsvgHandle *handle = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE, NULL, &error);
	RsvgHandlePtr handle(rsvg_handle_new_from_gfile_sync(file.get(), RSVG_HANDLE_FLAGS_NONE, NULL, &error));

    if (!handle)
    {
		g_printerr ("Error loading SVG `%s`: %s\n", full_path.string().c_str(), error->message);
		render_svg_missing(cr, center);
		return false;
    }

    rsvg_handle_set_dpi(handle.get(), 96.0);

	if (stylesheet.size() > 0)
	{
		bool set_style;
		set_style = rsvg_handle_set_stylesheet (handle.get(),
												reinterpret_cast<const uint8_t*>(stylesheet.c_str()),
												stylesheet.size(),
												&error);
		if (!set_style)
		{
			g_printerr ("error setting style: %s\n", error->message);
		}
		else
		{
			//std::cout << "Using style sheet:" << stylesheet << std::endl;
		}
	}

    // Get native size to maintain aspect ratio
    //double native_width, native_height;
    //rsvg_handle_get_intrinsic_size_in_pixels (handle, &native_width, &native_height);

    // Always render centered on lat/lon.
    // SVG should be set up so the reference marker
    // is at the center of the image
    //
    // Note, height seems to be ignored when svg has
    // size info so aspect ratio is preserved
	RsvgRectangle viewport = {
		.x = -width / 2.0, // position handled by translate later
		.y = -height / 2.0,
		.width = width,
		.height = height,
    };

	cairo_save(cr);
	cairo_translate(cr, center.x, center.y);
	cairo_rotate(cr, rotation * G_PI / 180.0);

    if (!rsvg_handle_render_document (handle.get(), cr, &viewport, &error))
    {
		g_printerr ("could not render: %s\n", error->message);
		render_svg_missing(cr, center);
		cairo_restore(cr);
		return false;
    }

	cairo_restore(cr);
    //std::cout << "Rendered SVG: " << full_path.string() << std::endl;

    return true;
}

/**
 * Plot a Big ? when we cant render an SVG
 */
void svg_collection::render_svg_missing(cairo_t *cr, coord center)
{
	char text[2] = "?";
    cairo_set_source_rgba(cr,
                          float(0) / 0xff,
                          float(0) / 0xff,
                          float(0) / 0xff,
                          float(255) / 0xff);
	cairo_select_font_face(cr, "monospace",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 35);

	cairo_text_extents_t text_extents = {};
    cairo_text_extents(cr, text, &text_extents);

	// draw sounding text without a subscript
	cairo_move_to(cr, center.x - text_extents.width/2, center.y + text_extents.height/2);
	cairo_show_text(cr, text);
}

}; // ~namespace encviz
