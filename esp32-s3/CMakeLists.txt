cmake_minimum_required(VERSION 3.16)
include(FetchContent)
FetchContent_Declare(
  espidflib
  GIT_REPOSITORY https://github.com/UncleRus/esp-idf-lib.git
)
#FetchContent_Declare(
#  HX711_IDF
#  GIT_REPOSITORY https://github.com/akshayvernekar/HX711_IDF.git
#)
#FetchContent_MakeAvailable(espidflib)
#FetchContent_MakeAvailable(HX711_IDF)
#set(EXTRA_COMPONENT_DIRS ${espidflib_SOURCE_DIR}/components)
#set(EXTRA_COMPONENT_DIRS ${HX711_IDF_SOURCE_DIR}/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(dankdryer)
