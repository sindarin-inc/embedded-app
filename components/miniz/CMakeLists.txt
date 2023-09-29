cmake_minimum_required(VERSION 3.5)

idf_component_register(
     SRCS miniz/miniz.c
     INCLUDE_DIRS "."
)

project(miniz)
