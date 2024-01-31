/*

  osmium_waterway_ids

  Extract ids of waterways and their nodes.

*/

#include <cstdlib>  // for std::exit
#include <cstring>  // for std::strncmp
#include <iostream> // for std::cout, std::cerr
#include <fstream>

// For the location index. There are different types of indexes available.
// This will work for all input files keeping the index in memory.
#include <osmium/index/map/flex_mem.hpp>

// For the NodeLocationForWays handler
#include <osmium/handler/node_locations_for_ways.hpp>

// The type of index used. This must match the include file above
using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

// The location handler always depends on the index type
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

// For assembling multipolygons
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_manager.hpp>

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

// For reading and parsing of tags filter
#include <osmium/index/nwr_array.hpp>
#include "util.hpp"

class WaterHandler : public osmium::handler::Handler {

    static void output_waterway(const osmium::Way& way, const char* tag_key, std::ofstream & out) {
        const osmium::TagList& tags = way.tags();
        const char* tag_value = tags.get_value_by_key(tag_key);
        if (nullptr != tag_value) {
            // Print id of the waterway and the tagname of the pub if it is set.
            out << way.id() << "," << tag_value;

            // Print ids of nodes of the ways
            for (const osmium::NodeRef& nr : way.nodes()) {
              out << "," << nr.ref();
            }

            out << std::endl;
        }
    }

    static void output_area(const osmium::Area& area, const char* tag_key, std::ofstream & out) {
        const osmium::TagList& tags = area.tags();
        const char* tag_value = tags.get_value_by_key(tag_key);
        if (nullptr != tag_value) {
            // Print id of the waterway and the tagname of the pub if it is set.
            out << area.orig_id() << "," << tag_value;

            // Because we set
            // create_empty_areas = false in the assembler config, we can
            // be sure there will always be at least one outer ring.

            // Print ids of nodes of the outer rings
            for (auto& ring : area.outer_rings()) {
                for (const osmium::NodeRef& nr : ring) {
                  out << "," << nr.ref();
                }
            }

            out << std::endl;
        }
    }

public:

    WaterHandler(const char* wayfile, const char* areafile)
        : m_filter(false)
    {
      waystream.open(wayfile);
      areastream.open(areafile);
    }

    ~WaterHandler() {
      waystream.close();
      areastream.close();
    }

    void way(const osmium::Way& way) {
        const osmium::TagList& tags = way.tags();
        if (osmium::tags::match_any_of(tags, m_filter)) {
            if (tags.has_key("waterway")) {
              output_waterway(way, "waterway", waystream);
            } else if (tags.has_key("natural")) {
              output_waterway(way, "natural", areastream);
            } else if (tags.has_key("landuse")) {
              output_waterway(way, "landuse", areastream);
            }
        }
    }

    void area(const osmium::Area& area) {
        const osmium::TagList& tags = area.tags();
        if (osmium::tags::match_any_of(tags, m_filter)) {
            if (tags.has_key("natural")) {
              output_area(area, "natural", areastream);
            } else if (tags.has_key("landuse")) {
              output_area(area, "landuse", areastream);
            }
        }
    }

    void parse_and_add_expression(const std::string& expression) {
        const auto p = get_filter_expression(expression);
        std::cout << "adding filter rule " << p.second << std::endl;
        m_filter.add_rule(true, get_tag_matcher(p.second));
    }

    void read_expressions_file(const std::string& file_name) {
        // Reading filter expressions file

        std::ifstream file{file_name};
        if (!file.is_open()) {
            throw std::runtime_error{"Could not open file '" + file_name + "'"};
        }

        for (std::string line; std::getline(file, line);) {
            const auto pos = line.find_first_of('#');
            if (pos != std::string::npos) {
                line.erase(pos);
            }
            if (!line.empty()) {
                if (line.back() == '\r') {
                    line.resize(line.size() - 1);
                }
                parse_and_add_expression(line);
            }
        }
    }

    const osmium::TagsFilter & getTagsFilter() {
        return m_filter;
    }

private:
    std::ofstream waystream;
    std::ofstream areastream;
    osmium::TagsFilter m_filter;

}; // class WaterHandler

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " osmfile.pbf tags-filter.txt wways.csv wtr.csv\n";
        std::exit(1);
    }

    try {
        // The input file
        const osmium::io::File input_file{argv[1]};

        // Create our waterway handler.
        WaterHandler data_handler(argv[3]/*wayfile*/, argv[4]/*areafile*/);
        data_handler.read_expressions_file(argv[2]/*tags-filter-file*/);

        // Configuration for the multipolygon assembler. We disable the option to
        // create empty areas when invalid multipolygons are encountered. This
        // means areas created have a valid geometry and invalid multipolygons
        // are simply ignored.
        osmium::area::Assembler::config_type assembler_config;
        assembler_config.create_empty_areas = false;

        // Initialize the MultipolygonManager. Its job is to collect all
        // relations and member ways needed for each area. It then calls an
        // instance of the osmium::area::Assembler class (with the given config)
        // to actually assemble one area.
        osmium::area::MultipolygonManager<osmium::area::Assembler> mp_manager{assembler_config, data_handler.getTagsFilter()};

        // We read the input file twice. In the first pass, only relations are
        // read and fed into the multipolygon manager.
        std::cerr << "Pass 1...\n";
        osmium::relations::read_relations(input_file, mp_manager);
        std::cerr << "Pass 1 done\n";

        // The index storing all node locations.
        index_type index;

        // The handler that stores all node locations in the index and adds them
        // to the ways.
        location_handler_type location_handler{index};

        // If a location is not available in the index, we ignore it. It might
        // not be needed (if it is not part of a multipolygon relation), so why
        // create an error?
        location_handler.ignore_errors();

        // On the second pass we read all objects and run them first through the
        // node location handler and then the multipolygon manager. The manager
        // will put the areas it has created into the "buffer" which are then
        // fed through our handler.
        //
        // The read_meta::no option disables reading of meta data (such as version
        // numbers, timestamps, etc.) which are not needed in this case. Disabling
        // this can speed up your program.
        std::cerr << "Pass 2...\n";
        osmium::io::Reader reader{input_file, osmium::io::read_meta::no};

        osmium::apply(reader, location_handler, data_handler, mp_manager.handler([&data_handler](const osmium::memory::Buffer& area_buffer) {
            osmium::apply(area_buffer, data_handler);
        }));

        reader.close();
        std::cerr << "Pass 2 done\n";
    } catch (const std::exception& e) {
        // All exceptions used by the Osmium library derive from std::exception.
        std::cerr << e.what() << '\n';
        std::exit(1);
    }
}
