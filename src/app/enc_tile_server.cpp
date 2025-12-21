/**
 * \file
 * \brief ENC Tile Server (WMTS)
 *
 * Minimal WMTS style tile server to dynamically render ENC(S-57) chart data
 * to PNG files, delivered over HTTP port 8888.
 *
 * NOTE: Set your tile server to:
 *   http://127.0.0.1:8888/<STYLE>/{z}/{y}/{x}.png
 *
 * Where "STYLE" is one of the defined chart styles (ie - "default"), and X/Y/Z
 * refer to the WTMS tile coordinates.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>
#include <encviz/enc_renderer.h>

#define PORT 8888

void usage(int exit_code)
{
    printf("Usage:\n"
           "  enc_tile_server [opts]\n"
           "\n"
           "Options:\n"
           "  -h         - Show help\n"
           "  -c <path>  - Set config file (default=~/.encviz/config.xml)\n");
    exit(exit_code);
}

std::vector<std::string> string_split(std::string input)
{
    std::vector<std::string> tokens;
    size_t pos;
    while ((pos = input.find('/')) != std::string::npos)
    {
        tokens.push_back(input.substr(0, pos));
        input = input.substr(pos + 1);
    }
    tokens.push_back(input);

    return tokens;
}

MHD_Result request_reply(MHD_Connection *conn, int code, const void *data, int len)
{
    MHD_Response *resp = MHD_create_response_from_buffer(len, (void*)data, MHD_RESPMEM_MUST_COPY);
    MHD_Result ret = MHD_queue_response(conn, code, resp);
    MHD_destroy_response(resp);
    printf(" - HTTP %d\n", code);
    return ret;
}

MHD_Result request_handler(void *cls, struct MHD_Connection *connection,
			   const char *url, const char *method,
			   const char *version, const char *upload_data,
			   size_t *upload_data_size, void **req_cls)
{
    // Parse URL
    printf("URL: %s\n", url);
    std::vector<std::string> tokens = string_split(url);
    if (tokens.size() != 5)
    {
	const char *msg = "Invalid URL";
	return request_reply(connection, MHD_HTTP_BAD_REQUEST,
			     msg, strlen(msg));
    }
    std::string style_name = tokens[1];
    int z = std::stoi(tokens[2]);
    int y = std::stoi(tokens[3]);
    int x = std::stoi(tokens[4]);

    // Get passed SQLite DB
    encviz::enc_renderer *enc_rend = (encviz::enc_renderer*)cls;

    // Render requested tile
    std::vector<uint8_t> out_bytes;
    printf("Tile X=%d, Y=%d, Z=%d\n", x, y, z);
    if (enc_rend->render(out_bytes, encviz::tile_coords::WTMS, x, y, z,
                         style_name.c_str()))
    {
        // Respond with rendered data
        return request_reply(connection, MHD_HTTP_OK,
                             out_bytes.data(), out_bytes.size());
    }
    else
    {
        // Nothing available, so 404 ...
        return request_reply(connection, MHD_HTTP_NOT_FOUND,
                             nullptr, 0);
    }
}

int main(int argc, char **argv)
{
    int opt;
    const char *config_file = nullptr;

    // Parse args
    while ((opt = getopt(argc, argv, "hc:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                // Help text
                usage(0);
                break;

            case 'c':
                // Set config path
                config_file = optarg;
                break;

            default:
                // Invalid arg / missing argument
                usage(1);
                break;
        }
    }

    // Global GDAL Initialization
    GDALAllRegister();

    // ENC renderer context
    encviz::enc_renderer enc_rend(config_file);

    // Start MHD
    MHD_Daemon *daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_THREAD_PER_CONNECTION,
					  PORT, NULL, NULL,
					  &request_handler, &enc_rend, MHD_OPTION_END);
    if (daemon == nullptr)
    {
		std::cerr << "Failed to start MHD Daemon!" << std::endl;
		return 1;
    }

	std::cerr << "Daemon Running, waiting for requests!" << std::endl;
    // Wait for input
    (void)getchar();

    // Cleanup
    MHD_stop_daemon (daemon);
    return 0;
}
