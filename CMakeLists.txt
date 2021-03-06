cmake_minimum_required(VERSION 2.8)

project(CukeBins)

set(CUKE_USE_STATIC_BOOST ${MSVC} CACHE BOOL "Statically link Boost (except boost::test)")
set(CUKE_DISABLE_BOOST_TEST OFF CACHE BOOL "Disable boost:test")
set(CUKE_DISABLE_CPPSPEC OFF CACHE BOOL "Disable CppSpec")
set(CUKE_DISABLE_GTEST OFF CACHE BOOL "Disable Google Test framework")

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

#
# Generic Compiler Flags
#

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Weffc++")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX") # exclude M$ min/max macros
    #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /analyze")
endif()

#
# Boost
#

set(CUKE_CORE_BOOST_LIBS thread system regex date_time)
if(NOT CUKE_DISABLE_BOOST_TEST)
    set(CUKE_TEST_BOOST_LIBS unit_test_framework)
endif()

if(CUKE_USE_STATIC_BOOST)
    set(CUKE_STATIC_BOOST_LIBS ${CUKE_CORE_BOOST_LIBS})
    # "An external test runner utility is required to link with dynamic library" (Boost User's Guide)
    set(CUKE_DYNAMIC_BOOST_LIBS ${CUKE_TEST_BOOST_LIBS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_TEST_DYN_LINK")

    if(NOT MSVC)
        find_package(Threads)
        set(CUKEBINS_EXTRA_LIBRARIES ${CUKEBINS_EXTRA_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
    endif()
else()
    set(CUKE_DYNAMIC_BOOST_LIBS ${CUKE_CORE_BOOST_LIBS} ${CUKE_TEST_BOOST_LIBS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_ALL_DYN_LINK")
endif()

if(CUKE_STATIC_BOOST_LIBS)
    set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost COMPONENTS ${CUKE_STATIC_BOOST_LIBS})
endif()

if(CUKE_DYNAMIC_BOOST_LIBS)
    set(Boost_USE_STATIC_LIBS OFF)
    find_package(Boost COMPONENTS ${CUKE_DYNAMIC_BOOST_LIBS})
endif()

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    set(CUKEBINS_EXTRA_LIBRARIES ${CUKEBINS_EXTRA_LIBRARIES} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY})
endif()

#
# CppSpec
#

if(NOT CUKE_DISABLE_CPPSPEC)
    find_package(CppSpec)
endif()

#
# GTest
#

if(NOT CUKE_DISABLE_GTEST)
    find_package(GTest)
endif()

#
# CukeBins
#

set(CUKEBINS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

include_directories(${CUKEBINS_INCLUDE_DIRS})

set(CUKEBINS_LIBRARIES CukeBins ${CUKEBINS_EXTRA_LIBRARIES})

add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)

