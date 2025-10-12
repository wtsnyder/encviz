/**
 * \file
 * \brief ENC Tile Render (Command Line)
 *
 * Minimal Command Line Interface (CLI) to render a single TMS tile.
 *
 * Note that this CLI tool uses the internal XYZ tile coordinates that start at
 * bottom left of map, instead of WTMS used by the tile server that starts at
 * top left.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <gdal.h>
#include <encviz/enc_dataset.h>
#include <encviz/enc_renderer.h>
#include <encviz/web_mercator.h>

void usage(int exit_code)
{
    printf("Usage:\n"
           "  enc_get_dataset \n"
           "\n");
    exit(exit_code);
}

int main(int argc, char **argv)
{
    // Global GDAL Initialization
    GDALAllRegister();

    encviz::enc_dataset enc_dataset;
    enc_dataset.load_charts("/home/will/charts/RI_ENCs/ENC_ROOT");

    // Create dataset
    GDALDataset *enc_data = GetGDALDriverManager()->GetDriverByName("Memory")->
      Create("", 0, 0, 0, GDT_Unknown, nullptr);

    // Bounds and zoom
    OGREnvelope bbox;
    bbox.MinX = -71.3;
    bbox.MaxX = -71.5;
    bbox.MinY = 41.4;
    bbox.MaxY = 41.5;
    int scale_min = 8000;

    enc_dataset.export_data(enc_data, {"LNDARE"}, bbox, scale_min);

    std::cout << "-----------RESULT----------------" << std::endl;

    OGRLayer *olayer = enc_data->GetLayerByName("LNDARE");
    for (const auto &feat : olayer)
    {
      std::cout << feat->DumpReadableAsString() << std::endl;
    }

    GDALDestroy();
    return 0;
}
