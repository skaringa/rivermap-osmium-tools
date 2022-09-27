
# OSM tools to help building the rivermap

See [Flussgebiete Mitteleuropas](https://www.kompf.de/gps/rivermap.html) for the intention and a more detailed description of this project.

Based on [libosmium](https://osmcode.org/libosmium/)

## Requires

You need a C++11 compliant compiler. GCC 4.8 and later as well as clang 3.4 and
later are known to work. You also need the following libraries:

    Osmium Library
        Need at least version 2.13.1
        https://osmcode.org/libosmium
        Debian/Ubuntu: libosmium2-dev

    Protozero
        Need at least version 1.5.1
        https://github.com/mapbox/protozero
        Debian/Ubuntu: libprotozero-dev

    gdalcpp
        https://github.com/joto/gdalcpp
        Included in the libosmium repository.

    bz2lib (for reading and writing bzipped files)
        http://www.bzip.org/
        Debian/Ubuntu: libbz2-dev

    CMake (for building)
        https://www.cmake.org/
        Debian/Ubuntu: cmake

    Expat (for parsing XML files)
        https://libexpat.github.io
        Debian/Ubuntu: libexpat1-dev
        openSUSE: libexpat-devel

    GDAL/OGR
        https://gdal.org/
        Debian/Ubuntu: libgdal-dev

    zlib (for PBF support)
        https://www.zlib.net/
        Debian/Ubuntu: zlib1g-dev
        openSUSE: zlib-devel

    PROJ
        https://proj.org/
        Debian/Ubuntu: libproj-dev

## Installing dependencies

### On Debian/Ubuntu

    apt-get install cmake libosmium2-dev libgdal-dev libproj-dev

In addition you might want to look at https://github.com/osmcode/osmium-proj if
you are using PROJ 6 or above.

## Building

[CMake](https://www.cmake.org) is used for building.

To build run:

    mkdir build
    cd build
    cmake ..
    make


## License

Available under the Boost Software License. See LICENSE.txt.


## Authors

Martin Kompf (https://www.kompf.de/)

Based on libosmium and OSM GIS Export that was mainly written and is maintained by Jochen Topf
(jochen@topf.org). 
