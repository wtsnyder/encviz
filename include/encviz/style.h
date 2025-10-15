#pragma once

/**
 * \file
 * \brief ENC Layer Styles
 *
 * Structures and definitions for loading information on styling ENC(S-57) data.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <tinyxml2.h>
#include <filesystem>
#include <map>

namespace encviz
{

/// Simple color map
struct color
{
    /// Alpha channel
    uint8_t alpha{255};

    /// Red channel (8 bit)
    uint8_t red{0};

    /// Blue channel (8 bit)
    uint8_t blue{0};

    /// Green channel (8 bit)
    uint8_t green{0};
};

enum MarkerShape
{
	CIRCLE_MARKER,
	SQUARE_MARKER
};

struct DepareColors
{
	color foreshore;
	color very_shallow;
	color medium_shallow;
	color medium_deep;
	color deep;
};

/// Style for a single layer
struct layer_style
{
    /// Name of layer
    std::string layer_name;

    /// Fill color
    color fill_color;

    /// Line color
    color line_color;

    /// Line width
    int line_width{1};

    /// Line dash style
    int line_dash;

    /// Circular marker radius, or box edge
    int marker_size;

	/// Shape of marker
	MarkerShape marker_shape;

	/// Colors only used for DEPARE layer
	DepareColors depare_colors;

    /// Text render attribute
    std::string attr_name;
};

typedef std::map<std::string, std::filesystem::path> IconStyle;

/// Full rendering style
struct render_style
{
    /// Background fill
    std::optional<color> background;

    /// Layers
    std::vector<layer_style> layers;

	/// SVG Icons
	IconStyle icons;
};

/**
 * Parse Color Code
 *
 * Color code pattern can be one of:
 *  - 4 bit RGB  : "f0f"
 *  - 4 bit ARGB : "ff0f"
 *  - 8 bit RGB  : "ff00ff"
 *  - 8 bit ARGB : "ffff00ff"
 *
 * \param[in] node Color code element
 * \return Parsed color code
 */
color parse_color(tinyxml2::XMLElement *node);

/**
 * Parse Layer Style
 *
 * \param[in] node Layer element
 * \return Parsed layer style
 */
layer_style parse_layer(tinyxml2::XMLElement *node);

 /**
  * Parse Icon
  */
std::pair<std::string, std::filesystem::path> parse_icon(tinyxml2::XMLElement *node, std::filesystem::path svg_path);

/**
 * Load Style from File
 *
 * \param[in] filename Path to style file
 * \return Loaded style
 */
render_style load_style(const std::string &filename, std::filesystem::path svg_path);

}; // ~namespace encviz
