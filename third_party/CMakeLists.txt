set(BOOST_ENABLE_CMAKE ON)
add_subdirectory(boost)

set(BUILD_GMOCK OFF CACHE INTERNAL "" FORCE)
set(INSTALL_GTEST OFF CACHE INTERNAL "" FORCE)
add_subdirectory(googletest)