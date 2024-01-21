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

add_executable(osmium_toogr osmium_toogr.cpp)
target_link_libraries(osmium_toogr ${OSMIUM_LIBRARIES})
set_pthread_on_target(osmium_toogr)
install(TARGETS osmium_toogr DESTINATION bin)

add_executable(osmium_toogr2 osmium_toogr2.cpp)
target_link_libraries(osmium_toogr2 ${OSMIUM_LIBRARIES})
set_pthread_on_target(osmium_toogr2)
install(TARGETS osmium_toogr2 DESTINATION bin)
