#pragma once

#include <iostream>
#include <sstream>
#include <string>

void StripHtml(std::stringstream &inStream, std::stringstream &outStream);
void StripHtml(std::string str, std::stringstream &outStream);
