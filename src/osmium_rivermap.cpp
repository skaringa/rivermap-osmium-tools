/*

  Tool to convert OSM water.pbf to Spatialite and to merge
  the names of river systems into it.

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
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <system_error>

#ifndef _MSC_VER
# include <unistd.h>
#endif

using index_type = osmium::index::map::Map<osmium::unsigned_object_id_type, osmium::Location>;
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

std::istream& comma(std::istream& is)
{
    char c;
    if (is >> c && c != ',')
        is.clear(std::ios::badbit);
    return is;
}

class RiversystemMap {

private:
    std::set<std::string> m_names;
    std::map<long, const char *> m_id2Name;
    std::string m_empty;

    void insert(long id, std::string name) {
        auto it = m_names.insert(name).first;
        const char * nameptr = it->c_str();
        m_id2Name.insert(std::pair<long, const char*>(id, nameptr));
    }

public:
    void load(const std::string& filename) {
        std::ifstream ifs;
        std::string header;
        ifs.open(filename);
        if (! ifs.eof()) {
            std::getline(ifs, header);
        }
        if (header.empty()) {
            throw std::runtime_error(std::string("Can't read from file ") + filename);
        }
        if (header != "id,rsystem") {
            throw std::runtime_error(std::string("Wrong csv header: ") + header);
        }

        long id;
        std::string name;
        while (! ifs.eof()) {
            ifs >> id >> comma >> name;
            //std::cout << id << ": " << name << std::endl;
            insert(id, name);
        }
        ifs.close();
    }

    const char * getName(long id) const {
        auto it = m_id2Name.find(id);
        if (it == m_id2Name.end()) {
            return m_empty.c_str();
        }
        return it->second;
    }

};

class MyOGRHandler : public osmium::handler::Handler {

    gdalcpp::Layer m_layer_linestring;
    RiversystemMap& m_rsystems;

    osmium::geom::OGRFactory<> m_factory;

public:
    explicit MyOGRHandler(gdalcpp::Dataset& dataset, RiversystemMap& rsystems) :
        m_layer_linestring(dataset, "waterway", wkbLineString),
        m_rsystems(rsystems) {

        m_layer_linestring.add_field("id", OFTReal, 10);
        m_layer_linestring.add_field("name", OFTString, 30);
        m_layer_linestring.add_field("type", OFTString, 30);
        m_layer_linestring.add_field("rsystem", OFTString, 30);
    }

    void way(const osmium::Way& way) {
        const char* waterway = way.tags().get_value_by_key("waterway");
        if (waterway) {
            try {
                const char* name = way.tags().get_value_by_key("name");
                gdalcpp::Feature feature{m_layer_linestring, m_factory.create_linestring(way)};
                feature.set_field("id", static_cast<double>(way.id()));
                if (name) {
                    feature.set_field("name", name);
                }
                feature.set_field("type", waterway);
                const char* riversystem = m_rsystems.getName(way.id());
                feature.set_field("rsystem", riversystem);
                feature.add_to_layer();
            } catch (const osmium::geometry_error&) {
                std::cerr << "Ignoring illegal geometry for way " << way.id() << ".\n";
            }
        }
    }

};

/* ================================================== */

void print_help() {
    std::cout << "osmium_rivermap [OPTIONS] [INFILE [OUTFILE]]\n\n" \
              << "If INFILE is not given stdin is assumed.\n" \
              << "If OUTFILE is not given 'ogr_out' is used.\n" \
              << "\nOptions:\n" \
              << "  -h, --help                 This help message\n" \
              << "  -l, --location_store=TYPE  Set location store\n" \
              << "  -f, --format=FORMAT        Output OGR format (Default: 'SQLite')\n" \
              << "  -r, --riversystems=FILE    Merge in riversystems csv file\n" \
              << "  -L                         See available location stores\n";
}

int main(int argc, char* argv[]) {
    try {
        const auto& map_factory = osmium::index::MapFactory<osmium::unsigned_object_id_type, osmium::Location>::instance();

        static struct option long_options[] = {
            {"help",                 no_argument,       nullptr, 'h'},
            {"format",               required_argument, nullptr, 'f'},
            {"location_store",       required_argument, nullptr, 'l'},
            {"riversystems",         required_argument, nullptr, 'r'},
            {"list_location_stores", no_argument,       nullptr, 'L'},
            {nullptr, 0, nullptr, 0}
        };

        std::string output_format{"SQLite"};
        std::string location_store{"flex_mem"};
        std::string rsystems_file;

        while (true) {
            const int c = getopt_long(argc, argv, "hf:l:r:L", long_options, nullptr);
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
                case 'r':
                    rsystems_file = optarg;
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

        RiversystemMap rsystems;
        if (! rsystems_file.empty()) {
            rsystems.load(rsystems_file);
        }
        MyOGRHandler ogr_handler{dataset, rsystems};

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

