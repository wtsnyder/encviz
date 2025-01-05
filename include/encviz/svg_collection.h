#pragma once

/**
 * \file
 * \brief SVG Collection
 *
 * C++ abstraction class to wrap a folder heiarchy of SVG files
 * and render them to a cairo context when needed
 */

#include <string>
#include <vector>
#include <filesystem>
#include <encviz/common.h>
#include <cairo.h>

namespace encviz
{

/// Wrapper class to handle SVG image requests
class svg_collection
{
public:

    /**
     * Constructor
     */
    svg_collection();

    /**
     * Set Svg Path
     *
     * \param[in] cache_path Specified cache path
     */
    void set_svg_path(const std::filesystem::path &svg_path);

    /**
     * Find the svg under the svg search path and render it
     * to the cairo context at location center and size [width, height]
     *
     */
    bool render_svg(cairo_t *cr, std::filesystem::path &svg_path,
		    coord center, double width, double height,
		    std::string stylesheet = "");
    
private:

    /// Root directory for searching for SVG files
    std::filesystem::path svg_root_path_;
    
};

}; // ~namespace encviz
