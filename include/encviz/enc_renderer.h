#pragma once

/**
 * \file
 * \brief ENC Renderer
 *
 * C++ abstraction class to handle visualization of ENC(S-57) chart data.
 */

#include <cstdint>
#include <string>
#include <filesystem>
#include <cairo.h>
#include <encviz/enc_dataset.h>
#include <encviz/style.h>
#include <encviz/web_mercator.h>
#include <encviz/svg_collection.h>

namespace encviz
{

class enc_renderer
{
public:

    /**
     * Constructor
     *
     * \param[in] config_path Path to configuration
     */
    enc_renderer(const char *config_path = nullptr);

    /**
     * Render Chart Data
     *
     * \param[out] data PNG bytestream
     * \param[in] tc Tile coordinate system (WMTS or XYZ)
     * \param[in] x Tile X coordinate (horizontal)
     * \param[in] y Tile Y coordinate (vertical)
     * \param[in] z Tile Z coordinate (zoom)
     * \param[in] style_name Name of style
     * \return False if no data to render
     */
    bool render(std::vector<uint8_t> &data, tile_coords tc,
                int x, int y, int z, const char *style_name);

private:

    /**
     * Render Feature Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_geo(cairo_t *cr, const OGRGeometry *geo,
                    const web_mercator &wm, const layer_style &style);

    /**
     * Render Depth Value
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_depth(cairo_t *cr, const OGRPoint *geo,
                      const web_mercator &wm, const layer_style &style);

    /**
     * Render Point Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_point(cairo_t *cr, const OGRPoint *geo,
                      const web_mercator &wm, const layer_style &style);

    /**
     * Render LineString Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_line(cairo_t *cr, const OGRLineString *geo,
                     const web_mercator &wm, const layer_style &style);

    /**
     * Render Polygon Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_poly(cairo_t *cr, const OGRPolygon *geo,
                     const web_mercator &wm, const layer_style &style);

    /**
     * Render a buoy with the right shape
     *
     */
    void render_buoy(cairo_t *cr, const OGRPoint *geo,
		     const web_mercator &wm, const layer_style &style,
		     const int buoy_shape,
		     std::vector<int> buoy_colors);

    /**
     * Render a beacon with the right shape
     *
     */
    void render_beacon(cairo_t *cr, const OGRPoint *geo,
		       const web_mercator &wm, const layer_style &style,
		       const int beacon_shape,
		       std::vector<int> beacon_colors);

    /**
     * Set Render Color
     *
     * \param[out] cr Image context
     * \param[in] c RGB color
     */
    void set_color(cairo_t *cr, const color &c);

    /**
     * Load Configuration
     *
     * \param[in] config_path
     */
    void load_config(const std::filesystem::path &config_path);

    /// Dimension of output image
    int tile_size_;

    /// Min display scale at zoom=0
    double min_scale0_;

    /// Chart collection
    enc_dataset enc_;

    /// Svg Collection
    svg_collection svg_;

    /// Loaded styles
    std::map<std::string, render_style> styles_;
};

}; // ~namespace encviz
