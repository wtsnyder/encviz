/**
 * \file
 * \brief Web Mercator
 *
 * C++ class to handle coordinate conversions between WGS84 (EPSG:4326), Web
 * Mercator (EPSG:3857), and WMS tiles.
 */

#include <cmath>
#include <encviz/web_mercator.h>

namespace encviz
{

/**
 * Constructor
 *
 * \param[in] x Tile x coordinate (TMS)
 * \param[in] y Tile y coordinate (TMS)
 * \param[in] y Tile z coordinate (TMS)
 * \param[in] tc Tile coordinate system (WTMS or XYZ)
 * \param[in] tile_size Tile side length in pixels
 * \param[in] tile_size Tile size in pixels
 */
web_mercator::web_mercator(std::size_t x, std::size_t y, std::size_t z,
                           tile_coords tc, int tile_size)
{
    // Nominal planet radius
    double radius = 6378137;

    // Nominal dimentions in meters of Web Mercator map at zoom level 0
    double tile_side = 2 * M_PI * radius;

    // Meter coordinates from bottom left, not center
    offset_m_ = tile_side / 2;

    // Number of tiles at this zoom level
    std::size_t ntiles = (std::size_t)pow(2, z);

    // All our math expects XYZ tile coordinates,
    // will need to flip Y axis if using WTMS
    if (tc == tile_coords::WTMS)
    {
        y = ntiles - y - 1;
    }

    // Update side length, resolution
    tile_side /= ntiles;

    // Compute bounding box (meters)
    bbox_m_.MinX = x * tile_side - offset_m_;
    bbox_m_.MinY = y * tile_side - offset_m_;
    bbox_m_.MaxX = bbox_m_.MinX + tile_side;
    bbox_m_.MaxY = bbox_m_.MinY + tile_side;

    // Compute pixels per meter
    ppm_ = tile_size / tile_side;
}

/**
 * Get bounding box for requested tile
 *
 * \return Computed coordinates
 */
OGREnvelope web_mercator::get_bbox_meters() const
{
    return bbox_m_;
}

/**
 * Get bounding box in degrees
 *
 * \return Computed coordinates
 */
OGREnvelope web_mercator::get_bbox_deg() const
{
    // Min/max coordinates
    coord cmin = { bbox_m_.MinX, bbox_m_.MinY };
    coord cmax = { bbox_m_.MaxX, bbox_m_.MaxY };

    // Convert coords to degrees
    cmin = meters_to_deg(cmin);
    cmax = meters_to_deg(cmax);

    // Return as envelope
    OGREnvelope bbox_deg;
    bbox_deg.MinX = cmin.x;
    bbox_deg.MaxX = cmax.x;
    bbox_deg.MinY = cmin.y;
    bbox_deg.MaxY = cmax.y;
    return bbox_deg;
}

/**
 * Convert coordinate from meters to degrees
 *
 * \param[in] in Input coordinate (degrees)
 * \return Output coordinate (meters)
 */
coord web_mercator::deg_to_meters(const coord &in) const
{
    coord out = {
        in.x * offset_m_ / 180.0,
        log(tan((90 + in.y) * M_PI / 360.0)) / (M_PI / 180.0)
    };
    out.y *= offset_m_ / 180.0;
    return out;
}

/**
 * Convert coordinate from meters to degrees
 *
 * \param[in] in Input coordinate (meters)
 * \return Output coordinate (degrees)
 */
coord web_mercator::meters_to_deg(const coord &in) const
{
    coord out = {
        (in.x / offset_m_) * 180,
        (in.y / offset_m_) * 180
    };
    out.y = 180 / M_PI * (2 * atan(exp(out.y * M_PI / 180)) - M_PI / 2);
    return out;
}

/**
 * Convert coordinate from meters to pixels
 *
 * \param[in] in Input coordinate (meters)
 * \return Output coordinate (pixels)
 */
coord web_mercator::meters_to_pixels(const coord &in) const
{
    // Compute meters from top left of tile, scaled by pixels per meter
    coord out = {
        (in.x - bbox_m_.MinX) * ppm_,
        (bbox_m_.MaxY - in.y) * ppm_
    };
    return out;
}

/**
 * Convert coordinate from pixels to meters
 *
 * \param[in] in Input coordinate (pixels)
 * \return Output coordinate (meters)
 */
coord web_mercator::pixels_to_meters(const coord &in) const
{
    // Compute meters from top left of tile, scaled by pixels per meter
    coord out = {
        bbox_m_.MinX + (in.x / ppm_),
        bbox_m_.MaxY - (in.y / ppm_)
    };
    return out;
}

/**
 * Convert OGR Point to pixels
 *
 * \param[in] point OGR point (deg)
 * \return Output coordinate (pixels)
 */
coord web_mercator::point_to_pixels(const OGRPoint &point) const
{
    // Convert lat/lon to pixel coordinates
    coord c = { point.getX(), point.getY() };
    c = deg_to_meters(c);
    return meters_to_pixels(c);
}

/**
 * Convert OGR Point to meters
 *
 * \param[in] point OGR point (deg)
 * \return Output coordinate (meters)
 */
coord web_mercator::point_to_meters(const OGRPoint &point) const
{
    // Convert lat/lon to m coordinates
    coord c = { point.getX(), point.getY() };
	return deg_to_meters(c);
}

}; // ~namespace encviz
