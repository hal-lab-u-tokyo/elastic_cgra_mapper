set(BOOST_ENABLE_CMAKE ON)
add_subdirectory(boost)

add_library(boost::numeric_ublas INTERFACE IMPORTED GLOBAL)
set_target_properties(boost::numeric_ublas PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/boost/libs/numeric/ublas/include)

set(BUILD_GMOCK OFF CACHE INTERNAL "" FORCE)
set(INSTALL_GTEST OFF CACHE INTERNAL "" FORCE)
add_subdirectory(googletest)