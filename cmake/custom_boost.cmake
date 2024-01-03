#
# Copyright (C) 2023 Codership Oy <info@codership.com>
#
#
OPTION(GALERA_CUSTOM_BOOST "Use custom boost version" OFF)
#
IF(NOT GALERA_CUSTOM_BOOST)
  RETURN()
ENDIF()
#
IF(NOT GALERA_BOOST_LIBRARIES)
  SET(GALERA_BOOST_LIBRARIES "all")
ENDIF()
#
IF(NOT GALERA_BOOST_DOWNLOAD_URL)

  IF(NOT GALERA_BOOST_VERSION)
    SET(GALERA_BOOST_VERSION "1.53.0")
  ENDIF()
  #
  STRING(REPLACE "." "_" GALERA_BOOST_TAR_VERSION ${GALERA_BOOST_VERSION})
  SET(GALERA_BOOST_PACKAGE_NAME "boost_${GALERA_BOOST_TAR_VERSION}.tar.bz2")
  SET(GALERA_LOCAL_BOOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libboost")
  SET(GALERA_LOCAL_BOOST_TAR "${CMAKE_CURRENT_SOURCE_DIR}/${GALERA_BOOST_PACKAGE_NAME}")
  SET(GALERA_BOOST_DOWNLOAD_URL
    "https://sourceforge.net/projects/boost/files/boost/${GALERA_BOOST_VERSION}/${GALERA_BOOST_PACKAGE_NAME}/download"
    )
  SET(GALERA_DOWNLOAD_BOOST_TIMEOUT 600 CACHE STRING "Timeout in seconds when downloading boost.")
ENDIF()
#
MACRO(GALERA_DOWNLOAD_BOOST)
  MESSAGE(STATUS "Downloading ${GALERA_BOOST_PACKAGE_NAME} to ${GALERA_LOCAL_BOOST_DIR}")
  FILE(DOWNLOAD ${GALERA_BOOST_DOWNLOAD_URL}
    ${CMAKE_CURRENT_SOURCE_DIR}/${GALERA_BOOST_PACKAGE_NAME}
    TIMEOUT ${GALERA_DOWNLOAD_BOOST_TIMEOUT}
    STATUS ERR
    SHOW_PROGRESS
    )
  IF(NOT ERR EQUAL 0)
    MESSAGE(STATUS "Download failed, error: ${ERR}")
    # A failed DOWNLOAD leaves an empty file, remove it
    FILE(REMOVE ${GALERA_LOCAL_BOOST_TAR})
    # STATUS returns a list of length 2
    LIST(GET ERR 0 NUMERIC_RETURN)
    IF(NUMERIC_RETURN EQUAL 28)
      MESSAGE(FATAL_ERROR
        "You can try downloading ${GALERA_BOOST_DOWNLOAD_URL} manually"
        " using curl/wget or a similar tool,"
        " or increase the value of GALERA_DOWNLOAD_BOOST_TIMEOUT"
        " (which is now ${GALERA_DOWNLOAD_BOOST_TIMEOUT} seconds)"
        )
    ENDIF()
    MESSAGE(FATAL_ERROR
      "You can try downloading ${GALERA_BOOST_DOWNLOAD_URL} manually"
      " using curl/wget or a similar tool"
      )
  ENDIF()
ENDMACRO()
#
MACRO(GALERA_UNPACK_BOOST_TARBALL)
  MESSAGE(STATUS "Unpacking ${GALERA_LOCAL_BOOST_TAR} to the ${GALERA_LOCAL_BOOST_DIR}")
  IF(NOT EXISTS ${GALERA_LOCAL_BOOST_DIR})
    FILE(MAKE_DIRECTORY ${GALERA_LOCAL_BOOST_DIR})
  ENDIF()
  EXECUTE_PROCESS(
    COMMAND ${CMAKE_COMMAND} -E tar xfj "${GALERA_LOCAL_BOOST_TAR}"
    WORKING_DIRECTORY "${GALERA_LOCAL_BOOST_DIR}"
    RESULT_VARIABLE tar_result
    )
  IF (tar_result MATCHES 0)
    SET(BOOST_FOUND 1 CACHE INTERNAL "")
  ELSE()
    MESSAGE(STATUS "Failed to extract files.\n"
      "   Please try downloading and extracting yourself.\n"
      "   The url is: ${GALERA_BOOST_DOWNLOAD_URL}")
    MESSAGE(FATAL_ERROR "Giving up.")
  ENDIF()
ENDMACRO()
#
SET(GALERA_BOOST_SRCDIR "${GALERA_LOCAL_BOOST_DIR}/boost_${GALERA_BOOST_TAR_VERSION}")
SET(GALERA_BOOST_VERSION_FILE "${GALERA_BOOST_SRCDIR}/boost/version.hpp")
IF(NOT EXISTS ${GALERA_BOOST_VERSION_FILE})
  IF(NOT EXISTS ${GALERA_LOCAL_BOOST_TAR})
    GALERA_DOWNLOAD_BOOST()
  ENDIF()
  GALERA_UNPACK_BOOST_TARBALL()
ENDIF()

FILE(STRINGS "${GALERA_BOOST_VERSION_FILE}"
  GALERA_BOOST_VERSION_NUMBER
  REGEX "^#define[\t ]+BOOST_VERSION[\t ][0-9]+.*"
  )
STRING(REGEX REPLACE
  "^.*BOOST_VERSION[\t ]([0-9])([0-9][0-9])([0-9][0-9]).*$" "\\1"
  GALERA_BOOST_MAJOR_VERSION "${GALERA_BOOST_VERSION_NUMBER}")
STRING(REGEX REPLACE
  "^.*BOOST_VERSION[\t ]([0-9][0-9])([0-9][0-9])([0-9][0-9]).*$" "\\2"
  GALERA_BOOST_MINOR_VERSION "${GALERA_BOOST_VERSION_NUMBER}")
STRING(REGEX REPLACE
  "^.*BOOST_VERSION[\t ]([0-9][0-9])([0-9][0-9])([0-9]).*$" "\\3"
  GALERA_BOOST_PATCH_VERSION "${GALERA_BOOST_VERSION_NUMBER}")
SET(CUSTOM_GALERA_BOOST_VERSION ${GALERA_BOOST_MAJOR_VERSION}.${GALERA_BOOST_MINOR_VERSION}.${GALERA_BOOST_PATCH_VERSION})
MESSAGE(STATUS "Custom boost VERSION_NUMBER is ${CUSTOM_GALERA_BOOST_VERSION}")

IF(NOT ${CUSTOM_GALERA_BOOST_VERSION} STREQUAL ${GALERA_BOOST_VERSION})
  MESSAGE(
    FATAL_ERROR 
    "Custom boost version in ${GALERA_BOOST_SRCDIR} is not specified version ${GALERA_BOOST_VERSION}\n"
    "Consider removing ${GALERA_BOOST_SRCDIR} and re-run cmake."
    )
ENDIF()
#
SET(GALERA_BOOST_BINDIR ${GALERA_LOCAL_BOOST_DIR}/boost_${GALERA_BOOST_TAR_VERSION}.bin)
IF(NOT EXISTS ${GALERA_BOOST_BINDIR})
  MESSAGE(STATUS "Building boost ${CUSTOM_GALERA_BOOST_VERSION} in ${GALERA_BOOST_SRCDIR}")
  # 
  EXECUTE_PROCESS(
    COMMAND ${GALERA_BOOST_SRCDIR}/bootstrap.sh --prefix=${GALERA_BOOST_BINDIR} --with-libraries=${GALERA_BOOST_LIBRARIES}
    WORKING_DIRECTORY "${GALERA_BOOST_SRCDIR}"
    RESULT_VARIABLE galera_boost_bootstrap_result
    )
  #
  IF(NOT ${galera_boost_bootstrap_result} MATCHES 0)
    MESSAGE(FATAL_ERROR "Boost ${CUSTOM_GALERA_BOOST_VERSION} build failed")
  ENDIF()
  #
  EXECUTE_PROCESS(
    COMMAND ${GALERA_BOOST_SRCDIR}/b2 install
    WORKING_DIRECTORY "${GALERA_BOOST_SRCDIR}"
    RESULT_VARIABLE galera_boost_build_result
    ) 
  #
  IF(NOT ${galera_boost_build_result} MATCHES 0)
    MESSAGE(FATAL_ERROR "Boost ${CUSTOM_GALERA_BOOST_VERSION} installation failed")
  ENDIF()
ENDIF()
#
SET(BOOST_ROOT ${GALERA_BOOST_BINDIR})
SET(Boost_NO_SYSTEM_PATHS ON)

