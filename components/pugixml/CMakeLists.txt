cmake_minimum_required(VERSION 3.5)

idf_component_register(
     SRCS src/pugixml.cpp
     INCLUDE_DIRS "src"
)

project(pugixml)
