/**
 * \file
 * \brief ENC Layer Styles
 *
 * Structures and definitions for loading information on styling ENC(S-57) data.
 */

#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <encviz/style.h>
#include <encviz/xml_config.h>

#define GET8(x, y) (0xff & ((x) >> (y)))
#define GET4(x, y) (0x11 * (0xf & ((x) >> (y))))

namespace encviz
{

/**
 * Print colors to css color format #RRGGBBAA
 */
std::ostream& operator<<(std::ostream& os, const color& c)
{
	std::stringstream ss;

	// output #RRGGBBAA
	ss << "#" << std::hex << std::setfill('0')
	   << std::setw(2) << int(c.red)
	   << std::setw(2) << int(c.green)
	   << std::setw(2) << int(c.blue)
	   << std::setw(2) << int(c.alpha);
	
    os << ss.str();
    return os;
}

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
color parse_color(tinyxml2::XMLElement *node)
{
    // Compute length of color code
    const char *code = xml_text(node);
    int len = strlen(code);

    // Ensure this parses correctly as a hex code
    char *eptr = nullptr;
    unsigned long bits = strtoul(code, &eptr, 16);
    if ((eptr - code) != len)
    {
        throw std::runtime_error("Invalid color code");
    }

    // Interpret bits
    color parsed;
    switch (len)
    {
        case 3:
            // 4 bit, 3 channel (RGB)
            parsed.alpha = 0xff;
            parsed.red   = GET4(bits, 8);
            parsed.green = GET4(bits, 4);
            parsed.blue  = GET4(bits, 0);
            break;

        case 4:
            // 4 bit, 4 channel (ARGB)
            parsed.alpha = GET4(bits, 12);
            parsed.red   = GET4(bits, 8);
            parsed.green = GET4(bits, 4);
            parsed.blue  = GET4(bits, 0);
            break;
            
        case 6:
            // 8 bit, 3 channel (RGB)
            parsed.alpha = 0xff;
            parsed.red   = GET8(bits, 16);
            parsed.green = GET8(bits, 8);
            parsed.blue  = GET8(bits, 0);
            break;

        case 8:
            // 8 bit, 4 channel (ARGB)
            parsed.alpha = GET8(bits, 24);
            parsed.red   = GET8(bits, 16);
            parsed.green = GET8(bits, 8);
            parsed.blue  = GET8(bits, 0);
            break;

        default:
            throw std::runtime_error("Invalid color code");
            break;
    }

    return parsed;
}

/**
 * Parse Layer Style
 *
 * \param[in] node Layer element
 * \return Parsed layer style
 */
layer_style parse_layer(tinyxml2::XMLElement *node, const std::filesystem::path &svg_path)
{
    // Sanity check
    if (node == nullptr)
    {
        throw std::runtime_error("Layer style may not be null");
    }

    // Parse layer XML
    layer_style parsed;
    parsed.layer_name = xml_text(xml_query(node, "layer_name"));
    parsed.fill_color = parse_color(xml_query(node, "fill_color"));
    parsed.line_color = parse_color(xml_query(node, "line_color"));
    parsed.line_width = atoi(xml_text(xml_query(node, "line_width")));
    parsed.line_dash = atoi(xml_text(xml_query(node, "line_dash")));
    parsed.marker_size = atoi(xml_text(xml_query(node, "marker_size")));
	parsed.marker_shape = CIRCLE_MARKER;
	tinyxml2::XMLElement* shape = node->FirstChildElement("marker_shape");
	if (shape)
	{
		std::string shape = xml_text(xml_query(node, "marker_shape"));
		if (shape == "square")
			parsed.marker_shape = SQUARE_MARKER;
		else if (shape == "circle")
			parsed.marker_shape = CIRCLE_MARKER;
	}

	parsed.icon_color = {255,0,0,0}; // black
	tinyxml2::XMLElement* icon_color = node->FirstChildElement("icon_color");
	if (icon_color)
	{
		parsed.icon_color = parse_color(xml_query(node, "icon_color"));
	}

	parsed.icon_size = 50;
	tinyxml2::XMLElement* icon_size = node->FirstChildElement("icon_size");
	if (icon_size)
	{
		parsed.icon_size = atoi(xml_text(xml_query(node, "icon_size")));
	}

	tinyxml2::XMLElement* icons = node->FirstChildElement("icons");
	if (icons)
	{
		std::cout << "Loading Icons:" << std::endl;
		for (tinyxml2::XMLElement *child : xml_query_all(icons, "icon"))
		{
			auto icon = parse_icon(child, svg_path);
			parsed.icons.insert(icon);
		}
	}

	tinyxml2::XMLElement* depare = node->FirstChildElement("depare_colors");
	if (depare)
	{
		parsed.depare_colors.foreshore = parse_color(xml_query(depare, "foreshore"));
		parsed.depare_colors.very_shallow = parse_color(xml_query(depare, "very_shallow"));
		parsed.depare_colors.medium_shallow = parse_color(xml_query(depare, "medium_shallow"));
		parsed.depare_colors.medium_deep = parse_color(xml_query(depare, "medium_deep"));
		parsed.depare_colors.deep = parse_color(xml_query(depare, "deep"));
	}
	
    return parsed;
}

/**
 * Parse Icon
 */
std::pair<std::string, std::filesystem::path> parse_icon(tinyxml2::XMLElement *node, std::filesystem::path svg_path)
{
	std::pair<std::string, std::filesystem::path> output;

	output.first = xml_text(xml_query(node, "name"));
	output.second = xml_text(xml_query(node, "file"));

	if (output.second.is_relative())
		output.second = svg_path / output.second;

	if (!std::filesystem::exists(output.second))
	{
		throw std::runtime_error("Unable to locate icon: " + output.second.string());
	}
	else
	{
		std::cout << "Found Icon: " << output.second << std::endl;
	}
	return output;
}

/**
 * Load Style from File
 *
 * \param[in] filename Path to style file
 * \return Loaded style
 */
render_style load_style(const std::string &filename, std::filesystem::path svg_path)
{
    // Load XML document
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filename.c_str()))
    {
        // Parse error?
        throw std::runtime_error("Cannot parse " + filename);
    }

    // Load Style
    tinyxml2::XMLElement *root = doc.RootElement();
    render_style parsed;
    try
    {
        parsed.background = parse_color(xml_query(root, "background"));
    }
    catch (...) {}
    for (tinyxml2::XMLElement *child : xml_query_all(root, "layer"))
    {
        parsed.layers.push_back(parse_layer(child, svg_path));
    }
	
	
    return parsed;
}

}; // ~namespace encviz
