cmake_minimum_required(VERSION 3.5)

idf_component_register(
    INCLUDE_DIRS "src"
    REQUIRES ulp
)

project(ulp-i2c-stub)