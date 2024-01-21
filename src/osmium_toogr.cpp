/*

  This is an example tool that converts OSM data to some output format
  like Spatialite or Shapefiles using the OGR library.

*/

#include <gdalcpp.hpp>

#include <osmium/geom/ogr.hpp>
#include <osmium/handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/all.hpp> // IWYU pragma: keep
#include <osmium/io/any_input.hpp> // IWYU pragma: keep
#include <osmium/visitor.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <system_error>

#ifndef _MSC_VER
# include <unistd.h>
#endif

using index_type = osmium::index::map::Map<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

class MyOGRHandler : public osmium::handler::Handler {

    gdalcpp::Layer* m_layer_places;
    gdalcpp::Layer* m_layer_peaks;
    gdalcpp::Layer* m_layer_roads;
    gdalcpp::Layer* m_layer_railways;
    gdalcpp::Layer* m_layer_boundaries;

    osmium::geom::OGRFactory<> m_factory;

public:

    explicit MyOGRHandler(gdalcpp::Dataset& dataset) {
        m_layer_places = new gdalcpp::Layer(dataset, "places", wkbPoint);
        m_layer_places->add_field("id", OFTReal, 10);
        m_layer_places->add_field("type", OFTString, 32);
        m_layer_places->add_field("name", OFTString, 32);

        m_layer_peaks = new gdalcpp::Layer(dataset, "peaks", wkbPoint);
        m_layer_peaks->add_field("id", OFTReal, 10);
        m_layer_peaks->add_field("type", OFTString, 32);
        m_layer_peaks->add_field("name", OFTString, 32);
        m_layer_peaks->add_field("importance", OFTString, 32);
        m_layer_peaks->add_field("ele", OFTString, 12);

        m_layer_roads = new gdalcpp::Layer(dataset, "roads", wkbLineString);
        m_layer_roads->add_field("id", OFTReal, 10);
        m_layer_roads->add_field("type", OFTString, 32);
        m_layer_roads->add_field("name", OFTString, 32);
        m_layer_roads->add_field("ref", OFTString, 16);

        m_layer_railways = new gdalcpp::Layer(dataset, "railways", wkbLineString);
        m_layer_railways->add_field("id", OFTReal, 10);

        m_layer_boundaries = new gdalcpp::Layer(dataset, "boundaries", wkbLineString);
        m_layer_boundaries->add_field("id", OFTReal, 10);
        m_layer_boundaries->add_field("level", OFTInteger, 4);
    }

    ~MyOGRHandler() {
        delete m_layer_places;
        delete m_layer_peaks;
        delete m_layer_roads;
        delete m_layer_railways;
        delete m_layer_boundaries;
    }

    void node(const osmium::Node& node) {
        const char* place = node.tags().get_value_by_key("place");
        const char* natural = node.tags().get_value_by_key("natural");
        if (place && (0 == std::strcmp(place, "town") || 0 == std::strcmp(place, "city"))) {
            gdalcpp::Feature feature{*m_layer_places, m_factory.create_point(node)};
            feature.set_field("id", static_cast<double>(node.id()));
            feature.set_field("type", place);
            feature.set_field("name", node.tags().get_value_by_key("name"));
            feature.add_to_layer();
        }
        else if (natural && 0 == std::strcmp(natural, "peak")) {
            gdalcpp::Feature feature{*m_layer_peaks, m_factory.create_point(node)};
            feature.set_field("id", static_cast<double>(node.id()));
            feature.set_field("type", natural);
            feature.set_field("name", node.tags().get_value_by_key("name"));
            feature.set_field("ele", node.tags().get_value_by_key("ele"));
            feature.set_field("importance", node.tags().get_value_by_key("importance"));
            feature.add_to_layer();
        }
    }

    void way(const osmium::Way& way) {
        const char* highway = way.tags().get_value_by_key("highway");
        const char* railway = way.tags().get_value_by_key("railway");
        const char* boundary = way.tags().get_value_by_key("boundary");
        if (highway && (0 == std::strcmp(highway, "motorway") || 0 == std::strcmp(highway, "motorway_link"))) {
            try {
                gdalcpp::Feature feature{*m_layer_roads, m_factory.create_linestring(way)};
                feature.set_field("id", static_cast<double>(way.id()));
                feature.set_field("type", highway);
                feature.set_field("name", way.tags().get_value_by_key("name"));
                feature.set_field("ref", way.tags().get_value_by_key("ref"));
                feature.add_to_layer();
            } catch (const osmium::geometry_error&) {
                std::cerr << "Ignoring illegal geometry for way " << way.id() << ".\n";
            }
        } else if (railway && 0 == std::strcmp(railway, "rail")) {
            try {
                gdalcpp::Feature feature{*m_layer_railways, m_factory.create_linestring(way)};
                feature.set_field("id", static_cast<double>(way.id()));
                feature.add_to_layer();
            } catch (const osmium::geometry_error&) {
                std::cerr << "Ignoring illegal geometry for way " << way.id() << ".\n";
            }
        } else if (boundary && 0 == std::strcmp(boundary, "administrative")) {
            try {
                gdalcpp::Feature feature{*m_layer_boundaries, m_factory.create_linestring(way)};
                feature.set_field("id", static_cast<double>(way.id()));
                const char* admin_level = way.tags().get_value_by_key("admin_level");
                feature.set_field("level", admin_level!=nullptr? atoi(admin_level) : 99);
                feature.add_to_layer();
            } catch (const osmium::geometry_error&) {
                std::cerr << "Ignoring illegal geometry for way " << way.id() << ".\n";
            }
        }
    }

};

/* ================================================== */

void print_help() {
    std::cout << "osmium_toogr [OPTIONS] [INFILE [OUTFILE]]\n\n" \
              << "If INFILE is not given stdin is assumed.\n" \
              << "If OUTFILE is not given 'ogr_out' is used.\n" \
              << "\nOptions:\n" \
              << "  -h, --help                 This help message\n" \
              << "  -l, --location_store=TYPE  Set location store\n" \
              << "  -f, --format=FORMAT        Output OGR format (Default: 'SQLite')\n" \
              << "  -L                         See available location stores\n";
}

int main(int argc, char* argv[]) {
    try {
        const auto& map_factory = osmium::index::MapFactory<osmium::unsigned_object_id_type, osmium::Location>::instance();

        static struct option long_options[] = {
            {"help",                 no_argument,       nullptr, 'h'},
            {"format",               required_argument, nullptr, 'f'},
            {"location_store",       required_argument, nullptr, 'l'},
            {"list_location_stores", no_argument,       nullptr, 'L'},
            {nullptr, 0, nullptr, 0}
        };

        std::string output_format{"SQLite"};
        std::string location_store{"flex_mem"};

        while (true) {
            const int c = getopt_long(argc, argv, "hf:l:L", long_options, nullptr);
            if (c == -1) {
                break;
            }

            switch (c) {
                case 'h':
                    print_help();
                    return 0;
                case 'f':
                    output_format = optarg;
                    break;
                case 'l':
                    location_store = optarg;
                    break;
                case 'L':
                    std::cout << "Available map types:\n";
                    for (const auto& map_type : map_factory.map_types()) {
                        std::cout << "  " << map_type << "\n";
                    }
                    return 0;
                default:
                    return 1;
            }
        }

        std::string input_filename;
        std::string output_filename{"ogr_out"};
        const int remaining_args = argc - optind;
        if (remaining_args > 2) {
            std::cerr << "Usage: " << argv[0] << " [OPTIONS] [INFILE [OUTFILE]]\n";
            return 1;
        }

        if (remaining_args == 2) {
            input_filename =  argv[optind];
            output_filename = argv[optind+1];
        } else if (remaining_args == 1) {
            input_filename =  argv[optind];
        } else {
            input_filename = "-";
        }

        osmium::io::Reader reader{input_filename};

        std::unique_ptr<index_type> index = map_factory.create_map(location_store);
        location_handler_type location_handler{*index};
        location_handler.ignore_errors();

        CPLSetConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");
        gdalcpp::Dataset dataset{output_format, output_filename, gdalcpp::SRS{}, { "SPATIALITE=TRUE", "INIT_WITH_EPSG=no" }};
        MyOGRHandler ogr_handler{dataset};

        osmium::apply(reader, location_handler, ogr_handler);
        reader.close();

        /*
        const int locations_fd = ::open("locations.dump", O_WRONLY | O_CREAT, 0644);
        if (locations_fd < 0) {
            throw std::system_error{errno, std::system_category(), "Open failed"};
        }
        index->dump_as_list(locations_fd);
        ::close(locations_fd);
        */
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

