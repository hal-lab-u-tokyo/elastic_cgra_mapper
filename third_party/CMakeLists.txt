set(BOOST_ENABLE_CMAKE ON)
add_subdirectory(boost)

if(NOT TARGET Eigen3::Eigen)
  # Prefer the Eigen submodule when present; otherwise use the system package
  # installed in the Docker image.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/eigen/Eigen")
    set(EIGEN3_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/eigen")
  endif()

  find_path(EIGEN3_INCLUDE_DIR Eigen/Eigen PATH_SUFFIXES eigen3)
  if(NOT EIGEN3_INCLUDE_DIR)
    message(
      FATAL_ERROR
        "Eigen3 not found. Install libeigen3-dev or provide third_party/eigen.")
  endif()

  add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
  set_target_properties(
    Eigen3::Eigen
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${EIGEN3_INCLUDE_DIR}
               INTERFACE_COMPILE_DEFINITIONS EIGEN_MPL2_ONLY)
endif()

set(BUILD_GMOCK
    OFF
    CACHE INTERNAL "" FORCE)
set(INSTALL_GTEST
    OFF
    CACHE INTERNAL "" FORCE)
add_subdirectory(googletest)
