#pragma once

/**
 * \file
 * \brief ENC Dataset
 *
 * C++ abstraction class to wrap and encapsulate all operations for searching
 * through ENC(S-57) datasets, indexing them, and extracting portions of that
 * data for later handling.
 */

#include <string>
#include <vector>
#include <filesystem>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>

namespace encviz
{

/// Wrapper class to handle ENC data requests
class enc_dataset
{
public:

    /// Per chart metadata
    struct metadata
    {
        /// Path to data file
        std::filesystem::path path;

        /// Compilation of scale (DSPM CSCL)
        int scale;

        /// Bounding box (deg)
        OGREnvelope bbox;
    };

    /**
     * Constructor
     */
    enc_dataset();

    /**
     * Set Cache Path
     *
     * \param[in] cache_path Specified cache path
     */
    void set_cache_path(const std::filesystem::path &cache_path);

    /**
     * Clear Chart Index
     */
    void clear();

    /**
     * Recursively Load ENC Charts
     *
     * \param[in] enc_root ENC_ROOT base directory
     */
    void load_charts(const std::string &enc_root);

    /**
     * Load Single ENC Chart
     *
     * \param[in] path Path to ENC chart
     * \return False on failure
     */
    bool load_chart(const std::filesystem::path &path);

    /**
     * Export ENC Data to Empty Dataset
     *
     * Creates specified layers in output dataset, populating with best data
     * available for given bounding box and minimum presentation scale.
     *
     * \param[out] ds Output dataset
     * \param[in] layers Specified ENC layers (S57)
     * \param[in] bbox Data bounding box (deg)
     * \param[in] scale_min Minimum data compilation scale
     * \return False if no data available
     */
    bool export_data(GDALDataset *ods, std::vector<std::string> layers,
                     OGREnvelope bbox, int scale_min);

private:

    /**
     * Save Single ENC Chart To Cache
     *
     * \param[in] path Path to ENC chart
     * \return False on failure
     */
    bool save_chart_cache(const metadata &meta);

    /**
     * Load Single ENC Chart From Cache
     *
     * \param[in] path Path to ENC chart
     * \return False on failure
     */
    bool load_chart_cache(const std::filesystem::path &path);

    /**
     * Load Single ENC Chart From Disk
     *
     * \param[in] path Path to ENC chart
     * \return False on failure
     */
    bool load_chart_disk(const std::filesystem::path &path);

    /**
     * Get OGR Integer Field
     *
     * \param[in] feat OGR feature
     * \param[in] name Field name
     * \return Requested value
     */
    int get_feat_field_int(OGRFeature *feat, const char *name);

    /**
     * Create Temporary Dataset
     *
     * \return Created dataset
     */
    std::unique_ptr<GDALDataset> create_temp_dataset();

    /**
     * Create Layer
     *
     * \param[out] ds Dataset for layer
     * \param[in] name Name of layer
     * \return Created dataset
     */
    OGRLayer *create_layer(GDALDataset *ds, const char *name);

    /**
     * Copy ENC Chart Coverage
     *
     * \param[out] layer Output layer
     * \param[in] ds Input dataset
     */
    void copy_chart_coverage(OGRLayer *layer, GDALDataset *ds);

    /**
     * Clear Layer Features
     *
     * \param[out] layer OGR layer
     */
    void clear_layer(OGRLayer *layer);

    /**
     * Bounding Box to Layer Feature
     *
     * \param[out] layer GDAL output layer
     * \param[in] bbox OGR Envelope
     */
    void create_bbox_feature(OGRLayer *layer, const OGREnvelope &bbox);

    /// Loaded chart metadata by chart name (stem)
    std::map<std::string, metadata> charts_;

    /// Chart data cache location
    std::filesystem::path cache_;

    /// GDAL memory driver handle
    GDALDriver *mem_drv_;
};

}; // ~namespace encviz
