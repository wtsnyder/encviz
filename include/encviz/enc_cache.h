#pragma once

/**
 * \file
 * \brief Enc Cache
 *
 * An object that stores recently loaded enc files as
 * loaded GDALDataset objects
 */

#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <encviz/common.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>

namespace encviz
{

struct GdalDeleter
{
    void operator()(GDALDataset* ds) const
    {
        if (ds) GDALClose(ds);
    }
};

using GDALDatasetPtr = std::unique_ptr<GDALDataset, GdalDeleter>;

/// Cache loaded enc files in memory
class EncCache
{
public:

    /**
     * Constructor
     */
    EncCache(int max_datasets, std::chrono::seconds max_age);

    /**
     * Try to find an enc dataset in the cache, or load it from disk
     * if not cached.
     *
     * \param[in] path to enc file
     */
    GDALDatasetPtr openFromCache(const std::filesystem::path &enc_path);

    /**
     * Clear out old or unused cached charts
     */
    void cleanCache();
    
private:

    struct CachedEnc
    {
        /// Time of last access
        std::chrono::steady_clock::time_point last_access;

        /// The dataset stored in memory
        GDALDatasetPtr dataset;
    };

    /// GDAL memory driver handle
    GDALDriver *mem_drv_;
    
    /// List of files currently being opened
    std::vector<std::string> opening_;

    /// Hashmap of cached datasets by filename
    std::unordered_map<std::string, CachedEnc> datasets_;

    /// Max allowed datasets in cache
    long unsigned int max_datasets_;

    /// Maximum age of a dataset in the cache
    std::chrono::seconds max_age_;

    /// Protect access to datasets_, dataset_count_, and access_times_
    std::mutex mutex_;

    /// Wait for file opens to complte
    std::condition_variable cv_;
};

}; // ~namespace encviz
