# Find the wazuh shared library
find_library(WAZUHEXT NAMES libwazuhext.dylib HINTS "${SRC_FOLDER}")
if(WAZUHEXT)
  set(uname "Darwin")
else()
  set(uname "Linux")
endif()
find_library(WAZUHEXT NAMES libwazuhext.so HINTS "${SRC_FOLDER}")

if(NOT WAZUHEXT)
    message(FATAL_ERROR "libwazuhext not found! Aborting...")
endif()

# # Add compiling flags and set tests dependencies
if(${uname} STREQUAL "Darwin")
    set(TEST_DEPS ${WAZUHLIB} ${WAZUHEXT} -lpthread -fprofile-arcs -ftest-coverage)
    add_compile_options(-ggdb -O0 -g -coverage -DTEST_AGENT -I/usr/local/include -DENABLE_SYSC -DWAZUH_UNIT_TESTING)
else()
    add_compile_options(-ggdb -O0 -g -coverage -DTEST_AGENT -DWAZUH_UNIT_TESTING)
    set(TEST_DEPS ${WAZUHLIB} ${WAZUHEXT} -lpthread -lcmocka -fprofile-arcs -ftest-coverage)
endif()

if(${uname} STREQUAL "Darwin")
  add_subdirectory(wazuh_modules)
else()
  add_subdirectory(client-agent)
endif()
