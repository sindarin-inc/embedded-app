#pragma once
#include <Arduino.h>

auto get_battery_life() -> String;

extern void (*resetFunc)();
