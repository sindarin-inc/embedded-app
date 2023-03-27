cmake_minimum_required(VERSION 3.5)

idf_component_register(
     SRCS src/ulp_riscv_i2c.c 
     INCLUDE_DIRS "src"
     REQUIRES ulp
)

project(ulp-i2c)
