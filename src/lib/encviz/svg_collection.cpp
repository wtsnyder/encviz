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

namespace encviz
{

svg_collection::svg_collection()
{

}

void svg_collection::set_svg_path(const std::filesystem::path &svg_path)
{
    svg_root_path_ = svg_path;
}

bool svg_collection::render_svg(cairo_t *cr, std::filesystem::path &svg_path,
				coord center, double width, double height)
{
    fs::path full_path = svg_root_path_;
    full_path /= svg_path;
    
    GError *error = NULL;
    GFile *file = g_file_new_for_path(full_path.string().c_str());

    RsvgHandle *handle = rsvg_handle_new_from_gfile_sync(file, RSVG_HANDLE_FLAGS_NONE, NULL, &error);

    rsvg_handle_set_dpi(handle, 96.0);

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
	.x = center.x - (width / 2.0),
	.y = center.y - (height / 2.0),
	.width = width,
	.height = height,
    };

    if (!rsvg_handle_render_document (handle, cr, &viewport, &error))
    {
	g_printerr ("could not render: %s", error->message);
	return false;
    }

    std::cout << "Rendered SVG: " << full_path.string() << std::endl;

    return true;
}

}; // ~namespace encviz
