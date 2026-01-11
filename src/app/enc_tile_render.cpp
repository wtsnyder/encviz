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
#include <gdal.h>
#include <encviz/enc_dataset.h>
#include <encviz/enc_renderer.h>
#include <encviz/web_mercator.h>

void usage(int exit_code)
{
    printf("Usage:\n"
           "  enc_tile_render [opts] <X> <Y> <Z>\n"
           "\n"
           "Options:\n"
           "  -h         - Show help\n"
           "  -c <path>  - Set config directory (default=~/.config)\n"
           "  -o <file>  - Set output file (default=out.png)\n"
           "  -s <name>  - Set render style (default=default)\n"
           "\n"
           "Where:\n"
           "  X          - Horizontal tile coordinate\n"
           "  Y          - Vertical tile coordinate\n"
           "  Z          - Zoom tile coordinate\n");
    exit(exit_code);
}

int main(int argc, char **argv)
{
    int opt;
    //encviz::tile_coords tc = encviz::tile_coords::XYZ;
    encviz::tile_coords tc = encviz::tile_coords::WTMS;
    std::string out_file = "out.png";
    const char *config_path = nullptr;
    const char *style_name = "default";

    // Parse args
    while ((opt = getopt(argc, argv, "hc:o:s:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                // Help text
                usage(0);
                break;

            case 'c':
                // Set config path
                config_path = optarg;
                break;

            case 'o':
                // Set output file
                out_file = optarg;
                break;

            case 's':
                // Set min display scale
                style_name = optarg;
                break;

            default:
                // Invalid arg / missing argument
                usage(1);
                break;
        }
    }
    if ((argc - optind) < 3)
    {
        usage(1);
    }
    int x = std::atoi(argv[optind + 0]);
    int y = std::atoi(argv[optind + 1]);
    int z = std::atoi(argv[optind + 2]);

    // Global GDAL Initialization
    GDALAllRegister();

    std::vector<uint8_t> png_bytes;
    encviz::enc_renderer enc_rend(config_path);
    enc_rend.render(png_bytes, tc, x, y, z, style_name);

    // Dump to file
    printf("Writing %lu bytes\n", png_bytes.size());
    FILE *ohandle = fopen(out_file.c_str(), "wb");
    fwrite(png_bytes.data(), 1, png_bytes.size(), ohandle);
    fclose(ohandle);

    GDALDestroy();
    return 0;
}
