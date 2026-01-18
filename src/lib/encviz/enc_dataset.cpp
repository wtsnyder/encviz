/**
 * \file
 * \brief ENC Dataset
 *
 * C++ abstraction class to wrap and encapsulate all operations for searching
 * through ENC(S-57) datasets, indexing them, and extracting portions of that
 * data for later handling.
 */

#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <encviz/enc_dataset.h>

// Helper macro for data presence
#define CHECKNULL(ptr, msg) if ((ptr) == nullptr) throw std::runtime_error((msg))

typedef std::unique_ptr<OGRGeometry, decltype(&OGRGeometryFactory::destroyGeometry)> GeoPtr;
typedef std::unique_ptr<OGRFeature, decltype(&OGRFeature::DestroyFeature)> FeatPtr;

namespace encviz
{

/**
 * Constructor
 */
enc_dataset::enc_dataset()
{
    // Cache location
    char *phome = getenv("HOME");
    if (phome != nullptr)
    {
        cache_ = phome;
        cache_ += "/.encviz";
    }

    // Get GDAL driver
    mem_drv_ = GetGDALDriverManager()->GetDriverByName("Memory");
    CHECKNULL(mem_drv_, "Cannot load OGR memory driver");
}

/**
 * Set Cache Path
 *
 * \param[in] cache_path Specified cache path
 */
void enc_dataset::set_cache_path(const std::filesystem::path &cache_path)
{
    cache_ = cache_path;
}

/**
 * Clear Chart Index
 */
void enc_dataset::clear()
{
    charts_.clear();
}

/**
 * Recursively Load ENC Charts
 *
 * \param[in] enc_root ENC_ROOT base directory
 */
void enc_dataset::load_charts(const std::string &enc_root)
{
    auto rdi = std::filesystem::recursive_directory_iterator(enc_root);
    for (const std::filesystem::directory_entry &entry : rdi)
    {
        if (entry.path().extension() == ".000")
        {
           load_chart(entry.path());
        }
    }

    printf("%lu charts loaded\n", charts_.size());
}

/**
 * Load Single ENC Chart
 *
 * \param[in] path Path to ENC chart
 */
bool enc_dataset::load_chart(const std::filesystem::path &path)
{
    return load_chart_cache(path) || load_chart_disk(path);
}

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
bool enc_dataset::export_data(GDALDataset *ods, std::vector<std::string> layers,
                              OGREnvelope bbox, int scale_min)
{
    printf("Filter: Scale=%d, BBOX=(%g to %g),(%g to %g)\n",
           scale_min, bbox.MinX, bbox.MaxX, bbox.MinY, bbox.MaxY);

    // Build list of suitable charts
    std::vector<const metadata*> selected;
    for (const auto &[name, chart] : charts_)
    {
        if ((scale_min <= chart.scale) && bbox.Intersects(chart.bbox))
        {
            selected.push_back(&chart);
        }
    }
    if (selected.empty())
    {
        return false;
    }

    // Sort in ascending scale order (most detailed first)
    std::sort(selected.begin(), selected.end(),
              [](const metadata* a, const metadata* b) {
                  return a->scale < b->scale;});

    // Dump what we have to screen
    printf("Selected %lu/%lu charts:\n", selected.size(), charts_.size());
    for (const auto &chart : selected)
    {
        printf(" - (%d) %s\n", chart->scale, chart->path.c_str());
    }

    // Create a new temp dataset for working
    auto temp_ds = create_temp_dataset();

    // Create layers in output dataset
    for (const std::string &layer_name : layers)
    {
        create_layer(ods, layer_name.c_str());
    };

    // Going to need 3 working layers
    OGRLayer *clip_layer = create_layer(temp_ds.get(), "");
    OGRLayer *coverage_layer = create_layer(temp_ds.get(), "");
    OGRLayer *result_layer = create_layer(temp_ds.get(), "");

    // Clip layer will track missing coverage
    create_bbox_feature(clip_layer, bbox);

    // Process charts one at a time to reduce repeated S57 parses
    for (const auto &chart : selected)
    {
        // Open input data set
        printf(" - Process: %s\n", chart->path.stem().string().c_str());
        GDALDataset *ids = GDALDataset::Open(chart->path.string().c_str(),
                                             GDAL_OF_VECTOR | GDAL_OF_READONLY,
                                             nullptr, nullptr, nullptr);
        CHECKNULL(ids, "Cannot open input data set");

        // Process chart's layers
        for (const std::string &layer_name : layers)
        {
            // NOTE: Some OGR drivers need to be done in sequence, and don't
            // like us jumping around betwen layers, like KML ...
            OGRLayer *olayer = ods->GetLayerByName(layer_name.c_str());
            if (olayer == nullptr)
            {
                throw std::runtime_error("Cannot open output layer (OGR interleaving issue?)");
            }

            // Get input layer
            OGRLayer *ilayer = ids->GetLayerByName(layer_name.c_str());
            if (ilayer == nullptr)
            {
                // Inland charts may not have certain features like depth
                // contours. If not present, just skip and move on
                continue;
            }
            
            // Certain layers we need to take the centroid of the polygon
            // so we can't crop out only the part we need
            if (layer_name == "TSSLPT"
                || layer_name == "ACHBRT"
                || layer_name == "LNDARE"
                || layer_name == "SEAARE"
                || layer_name == "BUAARE"
                || layer_name == "LNDRGN"
                || layer_name == "CBLSUB"
                //|| layer_name == "RESARE" //< consintent pattern spacing between tiles?
                || layer_name == "M_COVR")
            {
                // Copy all features from this layer
                for (const auto &feat : ilayer)
                {
                    // TODO! - This needs to check if the feature already exists
                    // from a neighboring chart and merge the geometry if it does
                    // instead of wholesale replacing the feature
                    GIntBig ifid = feat->GetFID();

                    // Get copy of the output feature
                    FeatPtr ofeat(olayer->GetFeature(ifid), &OGRFeature::DestroyFeature);
                    if (ofeat)
                    {
                        // already have this feature, merge the geometries
                        //std::cout << "repeat feature: " << ifid << std::endl;

                        // Steal geometry from output feature
                        GeoPtr ogeo(ofeat->StealGeometry(), &OGRGeometryFactory::destroyGeometry);
                        // Merge the geometries
                        OGRGeometry *uniongeo = ogeo->Union(feat->GetGeometryRef());
                        // Give the output feature copy its geometry back
                        ofeat->SetGeometryDirectly(uniongeo);
                        // Replace output layer feature with one with new geo
                        if (olayer->SetFeature(ofeat.get()) != OGRERR_NONE)
                        {
                            throw std::runtime_error("Cannot copy feature to output layer");
                        }
                        // ogeo is deleted, uniongeo now owned by the feature
                    }
                    else
                    {
                        // no feature in the output layer yet, just copy it over
                        if (olayer->SetFeature(feat.get()) != OGRERR_NONE)
                        {
                            throw std::runtime_error("Cannot copy feature to output layer");
                        }
                    }
                    
                }
            }
            else
            {
                // Clip out only the features in this tile for most layers
                if (ilayer->Clip(clip_layer, olayer) != OGRERR_NONE)
                {
                    throw std::runtime_error("Cannot perform layer clip operation");
                }
            }
        }

        // Remove any coverage from the clipping layer
        copy_chart_coverage(coverage_layer, ids);
        if (clip_layer->Erase(coverage_layer, result_layer) != OGRERR_NONE)
        {
            throw std::runtime_error("Cannot perform layer erase operation");
        }
        std::swap(clip_layer, result_layer);
        clear_layer(coverage_layer);
        clear_layer(result_layer);

        // Close input dataset
        delete ids;

        // Stop if all coverage is accounted for ...
        if (clip_layer->GetFeatureCount() == 0)
        {
            printf(" - Complete coverage (STOP)\n");
            break;
        }
    }

    return true;
}

void enc_dataset::print_layer(OGRLayer *layer)
{
    for (const auto &feat : layer)
    {
        std::cout << "Feature: " << std::string(feat->GetDefnRef()->GetName())
                  << " Fields : " << feat->GetDefnRef()->GetFieldCount() << std::endl;
        
        for (const auto &field : feat->GetDefnRef()->GetFields())
        {
            std::cout << std::string(field->GetNameRef()) << " : " << field->GetType() << "  ";
            switch (field->GetType())
            {
            case OFTInteger:
                std::cout << "int: " << feat->GetFieldAsInteger(field->GetNameRef());
                break;
            case OFTString:
                std::cout << "string: " << std::string(feat->GetFieldAsString(field->GetNameRef()));
                break;
            case OFTReal:
                std::cout << "double: " << double(feat->GetFieldAsDouble(field->GetNameRef()));
                break;
            default:
                std::cout << "other";
            }
            std::cout << std::endl;
        }
    }
}

/**
 * Save Single ENC Chart To Cache
 *
 * \param[in] path Path to ENC chart
 * \return False on failure
 */
bool enc_dataset::save_chart_cache(const metadata &meta)
{
    // Ensure cache directory exists
    if (!std::filesystem::exists(cache_) &&
        !std::filesystem::create_directories(cache_))
    {
        return false;
    }

    // Save to /path/to/cache/NAME ...
    std::filesystem::path cached_path = cache_ / meta.path.stem();
    std::ofstream handle(cached_path.string().c_str());
    if (handle.good())
    {
        handle << meta.path << "\n" << meta.scale << "\n"
               << meta.bbox.MinX << "\n" << meta.bbox.MaxX << "\n"
               << meta.bbox.MinY << "\n" << meta.bbox.MaxY << "\n";
    }

    return handle.good();
}

/**
 * Load Single ENC Chart from Cache
 *
 * \param[in] path Path to ENC chart
 * \return False on failure
 */
bool enc_dataset::load_chart_cache(const std::filesystem::path &path)
{
    // Look for /path/to/cache/NAME ...
    std::filesystem::path cached_path = cache_ / path.stem();
    if (std::filesystem::exists(cached_path))
    {
        // Try and read it
        std::ifstream handle(cached_path.string().c_str());
        if (handle.good())
        {
            metadata next;
            handle >> next.path >> next.scale
                   >> next.bbox.MinX >> next.bbox.MaxX
                   >> next.bbox.MinY >> next.bbox.MaxY;

            // Ensure no EOF, and path matches before saving metadata
            if (handle.good() && (path == next.path))
            {
                std::cout << "Load Chart bounds from cache: " << path << std::endl;
                charts_[path.stem().string()] = next;
                return true;
            }
        }
    }

    // Failed to load
    return false;
}

/**
 * Load Single ENC Chart from Disk
 *
 * \param[in] path Path to ENC chart
 * \return False on failure
 */
bool enc_dataset::load_chart_disk(const std::filesystem::path &path)
{
    metadata next = {path};

    // Open dataset
    std::cout << "Open Chart: " << path.string() << std::endl;
    const char *const drivers[] = { "S57", nullptr };
    GDALDataset *ds = GDALDataset::Open(path.string().c_str(),
                                        GDAL_OF_VECTOR | GDAL_OF_READONLY,
                                        drivers, nullptr, nullptr);
    CHECKNULL(ds, "Cannot open OGR dataset");

    // Get Compilation Scale of Chart
    {
        // Get "Dataset ID" (DSID) chart metadata
        OGRLayer *layer = ds->GetLayerByName("DSID");
        CHECKNULL(layer, "Cannot open DSID layer");

        // .. which contains one feature
        layer->ResetReading();
        std::unique_ptr<OGRFeature> feat(layer->GetNextFeature());
        CHECKNULL(feat, "Cannot read DSID feature");

        // .. that has "Dataset Parameter" (DSPM) "Compilation of Scale" (CSCL)
        next.scale = get_feat_field_int(feat.get(), "DSPM_CSCL");
    std::cout << "  scale: " << next.scale << std::endl;
    }

    // Get Chart Coverage Bounds
    {
        // Get "Coverage" (M_COVR) data
        OGRLayer *layer = ds->GetLayerByName("M_COVR");
        CHECKNULL(layer, "Cannot open M_COVR layer");

        // There's probably only one coverage feature,
        // but just in case, combine any we find
        for (auto &feat : layer)
        {
            // "Category of Coverage" (CATCOV) may be:
            //  - "coverage available" (1)
            //  - "no coverage available" (2)
            if (get_feat_field_int(feat.get(), "CATCOV") != 1)
                continue;

            // Get coverage for this feature
            OGRGeometry *geo = feat->GetGeometryRef();
            CHECKNULL(layer, "Cannot get feature geometry");

            // There's probably only one coverage feature,
            // but in case there's not merge each one
            OGREnvelope covr;
            geo->getEnvelope(&covr);
            next.bbox.Merge(covr);
        }

    std::cout << "  coverage X: " << next.bbox.MinX << "," << next.bbox.MaxX << std::endl;
    std::cout << "  coverage Y: " << next.bbox.MinY << "," << next.bbox.MaxY << std::endl;
    }

    // Cleanup
    delete ds;

    // Save
    charts_[path.stem().string()] = next;
    save_chart_cache(next);

    return true;
}

/**
 * Get OGR Integer Field
 *
 * \param[in] feat OGR feature
 * \param[in] name Field name
 * \return Requested value
 */
int enc_dataset::get_feat_field_int(OGRFeature *feat, const char *name)
{
    // See if field even exists
    int idx = feat->GetFieldIndex(name);
    if (idx == -1)
    {
        std::ostringstream oss;
        oss << "Feature does not have field \"" << name << "\"";
        throw std::runtime_error(oss.str());
    }

    // Check that the field is really an integer
    OGRFieldDefn *defn = feat->GetFieldDefnRef(idx);
    CHECKNULL(defn, "Cannot get feature field definition");
    if (defn->GetType() != OGRFieldType::OFTInteger)
    {
        std::ostringstream oss;
        oss << "Feature field \"" << name << "\" is not an integer";
        throw std::runtime_error(oss.str());
    }

    // Safe to call now
    return feat->GetFieldAsInteger(idx);
}

/**
 * Create Temporary Dataset
 *
 * \return Created dataset
 */
std::unique_ptr<GDALDataset> enc_dataset::create_temp_dataset()
{
    GDALDataset *ptr = mem_drv_->Create("", 0, 0, 0, GDT_Unknown, nullptr);
    CHECKNULL(ptr, "Cannot create temporary dataset");
    return std::unique_ptr<GDALDataset>(ptr);
}

/**
 * Create Temporary Layer
 *
 * \param[out] ds Dataset for layer
 * \param[in] name Name of layer
 * \return Created dataset
 */
OGRLayer *enc_dataset::create_layer(GDALDataset *ds, const char *name)
{
    OGRLayer *ptr = ds->CreateLayer(name);
    CHECKNULL(ptr, "Cannot create dataset layer");
    return ptr;
}


/**
 * Copy ENC Chart Coverage
 *
 * \param[out] layer Output layer
 * \param[in] ds Input dataset
 */
void enc_dataset::copy_chart_coverage(OGRLayer *layer, GDALDataset *ds)
{
    // Get "Coverage" (M_COVR) data
    OGRLayer *ilayer = ds->GetLayerByName("M_COVR");
    CHECKNULL(ilayer, "Cannot open M_COVR layer");

    // Copy any defined coverage features
    for (auto &feat : ilayer)
    {
        // "Category of Coverage" (CATCOV) may be:
        //  - "coverage available" (1)
        //  - "no coverage available" (2)
        if (get_feat_field_int(feat.get(), "CATCOV") != 1)
            continue;

        // Create output feature
        if (layer->CreateFeature(feat.get()) != OGRERR_NONE)
        {
            throw std::runtime_error("Cannot create coverage feature");
        }
    }
}

/**
 * Clear Layer Features
 *
 * \param[out] layer OGR layer
 */
void enc_dataset::clear_layer(OGRLayer *layer)
{
    for (auto &feat : layer)
    {
        if (layer->DeleteFeature(feat->GetFID()) != OGRERR_NONE)
        {
            throw std::runtime_error("Cannot delete layer feature");
        }
    }
}

/**
 * Bounding Box to Layer Feature
 *
 * \param[out] layer GDAL output layer
 * \param[in] bbox OGR Envelope
 */
void enc_dataset::create_bbox_feature(OGRLayer *layer, const OGREnvelope &bbox)
{
    // Define polygon boundary first
    OGRLinearRing geo_ring;
    geo_ring.addPoint(bbox.MinX, bbox.MinY);
    geo_ring.addPoint(bbox.MaxX, bbox.MinY);
    geo_ring.addPoint(bbox.MaxX, bbox.MaxY);
    geo_ring.addPoint(bbox.MinX, bbox.MaxY);
    geo_ring.addPoint(bbox.MinX, bbox.MinY);

    // Define the polygon
    OGRPolygon geo_poly;
    geo_poly.addRing(&geo_ring);

    // Create a feature based on the output layer
    OGRFeature feat(layer->GetLayerDefn());
    feat.SetGeometry(&geo_poly);

    // Add to layer
    if (layer->CreateFeature(&feat) != OGRERR_NONE)
    {
        throw std::runtime_error("Cannot create layer feature");
    }
}

}; // ~namespace encviz
