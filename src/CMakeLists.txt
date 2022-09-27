#-----------------------------------------------------------------------------
#
#  CMake Config
#
#-----------------------------------------------------------------------------

add_executable(osmium_rivermap osmium_rivermap.cpp)
target_link_libraries(osmium_rivermap ${OSMIUM_LIBRARIES})
set_pthread_on_target(osmium_rivermap)
install(TARGETS osmium_rivermap DESTINATION bin)


add_executable(osmium_waterway_ids osmium_waterway_ids.cpp)
target_link_libraries(osmium_waterway_ids ${OSMIUM_LIBRARIES})
set_pthread_on_target(osmium_waterway_ids)
install(TARGETS osmium_waterway_ids DESTINATION bin)
