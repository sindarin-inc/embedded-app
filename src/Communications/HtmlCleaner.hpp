#pragma once

#include <Stream.h>
#include <iostream>
#include <sstream>
#include <string>

// TODO: A Stream<->std::stringstream adapter class could simplify this
void StripHtml(std::stringstream &inStream, std::stringstream &outStream);
void StripHtml(const std::string &str, std::stringstream &outStream);
void StripHtml(Stream &inStream, std::stringstream &outStream);
void StripHtml(std::stringstream &inStream, Stream &outStream);
void StripHtml(const std::string &str, Stream &outStream);
