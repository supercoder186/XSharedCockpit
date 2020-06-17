cmake_minimum_required(VERSION 3.12)

# Build system configuration
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED 11)

# Project configuration

project(XSharedCockpit)

set(XSharedCockpit_VERSION 1.3.0)

# Plugin configuration

file(GLOB_RECURSE PLUGIN_SRC "plugin.cpp")
add_library(plugin SHARED ${PLUGIN_SRC})

set_target_properties(plugin PROPERTIES OUTPUT_NAME "XSharedCockpit")
set_target_properties(plugin PROPERTIES SUFFIX ".xpl")
set_target_properties(plugin PROPERTIES PREFIX "")

# XSDK configuration

set(XSDK_DIR CACHE PATH "Path to the X-Plane SDK")
set(XSDK_LIBRARIES "${XSDK_DIR}/Libraries")
set(XSDK_INCLUDE "${XSDK_DIR}/CHeaders")
add_definitions(-DXPLM200 -DXPLM210 -DXPLM300 -DXPLM301)

# OS-Specifics

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(SYSTEM_NAME "mac_x64")
  add_definitions(-DAPL=1)
  set(XSDK_LIBRARIES "-F ${XSDK_LIBRARIES}/Mac -framework XPLM -framework XPWidgets")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(SYSTEM_NAME "linux_x64")
  add_definitions(-DLIN=1)
  target_link_options(plugin PRIVATE "-shared -fvisibility=hidden -rdynamic -nodefaultlibs -undefined_warning")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SYSTEM_NAME "win_x64")
  add_definitions(-DIBM=1)
  link_directories("${XSDK_LIBRARIES}/Win/")
  set(XSDK_LIBRARIES "XPLM_64 -lXPWidgets_64")
endif()

target_link_libraries(plugin ${XSDK_LIBRARIES})

# Modules
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.72.0 REQUIRED)

# Headers
target_include_directories(plugin PUBLIC "${XSDK_INCLUDE}/XPLM")
target_include_directories(plugin PUBLIC "${XSDK_INCLUDE}/XPWidgets")
target_include_directories(plugin PUBLIC ${Boost_INCLUDE_DIRS})

install(TARGETS plugin DESTINATION "XSharedCockpit/${SYSTEM_NAME}")