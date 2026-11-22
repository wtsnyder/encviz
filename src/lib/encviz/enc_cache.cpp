/**
 * \file
 * \brief Enc Cache
 *
 * An object that stores recently loaded enc files as
 * loaded GDALDataset objects
 */

#include <encviz/enc_cache.h>
#include <algorithm>
#include <map>
#include <format>
#include <iostream>

namespace encviz
{

/**
 * Constructor
 */
EncCache::EncCache(int max_datasets, std::chrono::seconds max_age)
    : max_datasets_(max_datasets)
    , max_age_(max_age)
                    
{
    // Get GDAL driver
    mem_drv_ = GetGDALDriverManager()->GetDriverByName("Memory");
    if (mem_drv_ == nullptr)
        throw std::runtime_error("Cannot load OGR memory driver");
}

GDALDatasetPtr EncCache::openFromCache(const std::filesystem::path &enc_path)
{
    std::unique_lock<std::mutex> lock(mutex_);

    std::string path_str = enc_path.string();
    
    // copy of our opened dataset
    GDALDatasetPtr ds_copy;

    // while dataset being openend on another thread
    while (std::find(opening_.begin(), opening_.end(), path_str) != opening_.end())
    {
        std::cout << "Waiting on another thread to open: " << path_str << std::endl;
        // wait until another thread finished opening our file
        cv_.wait(lock);
    }

    auto it = datasets_.find(enc_path);
    if (it != datasets_.end())
    {
        std::cout << "Found cached: " << path_str << std::endl;
        // found dataset
        // update access time
        it->second.last_access = std::chrono::steady_clock::now();
        // create copy
        ds_copy.reset(static_cast<GDALDataset*>(mem_drv_->CreateCopy(
                                                                     path_str.c_str(),
                                                                     it->second.dataset.get(),
                                                                     false,
                                                                     nullptr,
                                                                     nullptr,
                                                                     nullptr)));
    }
    else
    {
        // add to list of opening files
        opening_.push_back(path_str);
        std::cout << "Opening: " << path_str << std::endl;
        auto open_start = std::chrono::high_resolution_clock::now();
        
        // release mutex so file opens can run simultaneously
        lock.unlock();

        // open dataset
        CachedEnc new_enc;
        new_enc.dataset.reset(GDALDataset::Open(path_str.c_str(),
                                                GDAL_OF_VECTOR | GDAL_OF_READONLY,
                                                nullptr, nullptr, nullptr));
        new_enc.last_access = std::chrono::steady_clock::now();
        if (!new_enc.dataset)
            throw std::runtime_error("Cannot open input data set");

        // get mutex back
        lock.lock();

        auto open_complete = std::chrono::high_resolution_clock::now();
        auto open_duration = std::chrono::duration_cast<std::chrono::microseconds>(open_complete - open_start);
        std::string open_time = std::format("{:.3f} msec", open_duration.count() / 1000.0);
        std::cout << "Open complete: " << path_str << " in " << open_time << std::endl;
        
        // remove from list of opening files
        std::erase_if(opening_, [path_str](std::string x) { return x == path_str;});
        // create copy
        ds_copy.reset(static_cast<GDALDataset*>(mem_drv_->CreateCopy(
                                                                     path_str.c_str(),
                                                                     new_enc.dataset.get(),
                                                                     false,
                                                                     nullptr,
                                                                     nullptr,
                                                                     nullptr)));
        // add to cache
        datasets_[path_str] = std::move(new_enc);

        // notify waiting threads that an opening dataset has been removed from opening_
        cv_.notify_all();
    }

    cleanCache();

    return ds_copy; 

}

void EncCache::cleanCache()
{
    // Sort cached datasets by last access time
    auto steady_now = std::chrono::steady_clock::now();
    std::map<std::chrono::steady_clock::duration, std::string> sorted_datasets;
    for (const auto& [filename, enc] : datasets_)
    {
        sorted_datasets[(steady_now - enc.last_access)] = filename;
    }

    // Loop datasets oldest to newest
    for (auto it = sorted_datasets.rbegin();
         it != sorted_datasets.rend();
         ++it)
    {
        // done if oldest dataset is still below max age and dataset count low enough
        if (it->first < max_age_ && datasets_.size() <= max_datasets_)
            break;

        // remove dataset
        datasets_.erase(it->second);
    }

    std::cout << "Cached datasets: " << datasets_.size() << std::endl;
}

}
