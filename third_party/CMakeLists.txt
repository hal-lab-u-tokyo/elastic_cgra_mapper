set(BOOST_ENABLE_CMAKE ON)
add_subdirectory(boost)

add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
set_target_properties(
  Eigen3::Eigen
  PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/eigen
             INTERFACE_COMPILE_DEFINITIONS EIGEN_MPL2_ONLY)

set(BUILD_GMOCK
    OFF
    CACHE INTERNAL "" FORCE)
set(INSTALL_GTEST
    OFF
    CACHE INTERNAL "" FORCE)
add_subdirectory(googletest)
