// Based on example here:
// https://github.com/zenmanenergy/ESP8266-Arduino-Examples/blob/master/helloWorld_urlencoded/urlencode.ino

#include <string>

auto Urldecode(const std::string &str) -> std::string;
auto Urlencode(const std::string &str) -> std::string;
