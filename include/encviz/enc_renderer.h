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
    enc_renderer(const char *config_file = nullptr);

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
     * \param[out] phase Phase tracking for multi-line strings
     * \param[in] coverage_polygons Lines will not be rendered where they overlap with
     *                              with coverage bounds
     */
    void render_geo(cairo_t *cr, const OGRGeometry *geo,
                    const web_mercator &wm, const layer_style &style,
                    double &phase,
                    OGRGeometry *coverage_polygons);

    /**
     * Render multi-polygons
     */
    void render_multipoly(cairo_t *cr, const OGRGeometry *geo,
                          const web_mercator &wm, const layer_style &style);
    
    /**
     * Render Depth Value
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_depth(cairo_t *cr, const OGRPoint *geo, double depth,
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
     * \param[in/out] phase Phase of dashing for connecting multiple segments
     */
    void render_line(cairo_t *cr, const OGRLineString *geo,
                     const web_mercator &wm, const layer_style &style, double &phase);
    
    /**
     * Render LineString Geometry as basic solid or dashed lines
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     * \param[in/out] phase Phase of dashing for connecting multiple segments
     */
    void render_line_basic(cairo_t *cr, const OGRLineString *geo,
                           const web_mercator &wm, const layer_style &style, double &phase);

    /**
     * Render LineString With Points Geometry
     * Put a dot at every point allong line, mostly for debug purposes
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_line_with_dots(cairo_t *cr, const OGRLineString *geo,
                               const web_mercator &wm, const layer_style &style);

    /**
     * Render Wavy LineString Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     * \param[in/out] phase Phase of the sin wave for connecting multiple segments
     */
    void render_line_wavy(cairo_t *cr, const OGRLineString *geo,
                          const web_mercator &wm, const layer_style &style, double &phase);

    /**
     * Render LineString Geometry with T Dashed lines
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     * \param[in/out] phase Phase of the sin wave for connecting multiple segments
     */
    void render_line_dash_t(cairo_t *cr, const OGRLineString *geo,
                            const web_mercator &wm, const layer_style &style, double &phase);

    /**
     * Render LineString Geometry with Triangle Dashed lines
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     * \param[in/out] phase Phase of the sin wave for connecting multiple segments
     */
    void render_line_dash_triangles(cairo_t *cr, const OGRLineString *geo,
                                    const web_mercator &wm, const layer_style &style, double &phase);

    /**
     * Render Polygon Geometry, just filled in polygon, no borders
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     */
    void render_poly(cairo_t *cr, const OGRPolygon *geo,
                     const web_mercator &wm, const layer_style &style);

    /**
     * Render Polygon Border Geometry
     *
     * \param[out] cr Image context
     * \param[in] geo Feature geometry
     * \param[in] wm Web Mercator point mapper
     * \param[in] style Feature style
     * \param[in] coverage_polygons Lines will not be rendered where they overlap with
     *                              with coverage bounds
     */
    void render_poly_borders(cairo_t *cr, const OGRPolygon *geo,
                             const web_mercator &wm, const layer_style &style,
                             const OGRGeometry *coverage_polygons = nullptr);

    /**
     * Render depth areas with special colors
     *
     */
    void render_depare(cairo_t *cr, const OGRPolygon *geo,
                     const web_mercator &wm, const layer_style &style,
                     const OGRFeature *feat);
    
    /**
     * Render a buoy with the right shape
     *
     */
    void render_buoy(cairo_t *cr, const OGRPoint *geo,
                     const web_mercator &wm, const layer_style &style,
                     const OGRFeature *feat);

    /**
     * Render a beacon with the right shape
     *
     */
    void render_beacon(cairo_t *cr, const OGRPoint *geo,
                       const web_mercator &wm, const layer_style &style,
                       const OGRFeature *feat);

    /**
     * Render a fog signal
     *
     */
    void render_fog(cairo_t *cr, const OGRPoint *geo,
                    const web_mercator &wm, const layer_style &style,
                    const OGRFeature *feat);

    /**
     * Render a light
     *
     */
    void render_light(cairo_t *cr, const OGRPoint *geo,
                      const web_mercator &wm, const layer_style &style,
                      const OGRFeature *feat);

    /**
     * Render landmarks
     *
     */
    void render_landmark(cairo_t *cr, const OGRPoint *geo,
                         const web_mercator &wm, const layer_style &style,
                         const OGRFeature *feat);

    /**
     * Render silo/tank
     *
     */
    void render_silotank(cairo_t *cr, const OGRPoint *geo,
                         const web_mercator &wm, const layer_style &style,
                         const OGRFeature *feat);

    /**
     * Render a rock using an icon
     *
     */
    void render_rock(cairo_t *cr, const OGRPoint *geo,
                     const web_mercator &wm, const layer_style &style,
                     const OGRFeature *feat);

    /**
     * Render an obstruction using an icon
     *
     */
    void render_obstruction(cairo_t *cr, const OGRPoint *geo,
                            const web_mercator &wm, const layer_style &style,
                            const OGRFeature *feat);

    /**
     * Render a wreck using an icon
     *
     */
    void render_wreck(cairo_t *cr, const OGRPoint *geo,
                      const web_mercator &wm, const layer_style &style,
                      const OGRFeature *feat);

    /**
     * Render an archor berth using an icon
     *
     */
    void render_anchor(cairo_t *cr, const OGRPoint *geo,
                      const web_mercator &wm, const layer_style &style,
                      const OGRFeature *feat);

    /**
     * Experiment with TSSLPT
     */
    void render_traffic_sep_part(cairo_t *cr, const OGRPolygon *geo,
                                 const web_mercator &wm, const layer_style &style,
                                 const OGRFeature *feat);

    /**
     * Show the name of a named area at the centroid of the piece of land
     */
    void render_named_area(cairo_t *cr, const OGRPolygon *geo,
                           const web_mercator &wm, const layer_style &style,
                           const OGRFeature *feat);

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
     * \param[in] config_file
     */
    void load_config(const std::filesystem::path &config_file);

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
