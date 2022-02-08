// Based on example here:
// https://github.com/zenmanenergy/ESP8266-Arduino-Examples/blob/master/helloWorld_urlencoded/urlencode.ino

#include "UrlEncode.hpp"

auto H2int(char c) -> unsigned char {
    if (c >= '0' && c <= '9') {
        return ((unsigned char)c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return ((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return ((unsigned char)c - 'A' + 10);
    }
    return (0);
}

auto Urldecode(const std::string &str) -> std::string {

    std::string encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.at(i);
        if (c == '+') {
            encodedString += ' ';
        } else if (c == '%') {
            i++;
            code0 = str.at(i);
            i++;
            code1 = str.at(i);
            c = (H2int(code0) << 4) | H2int(code1);
            encodedString += c;
        } else {

            encodedString += c;
        }
    }

    return encodedString;
}

auto Urlencode(const std::string &str) -> std::string {
    std::string encodedString("");
    char c;
    char code0;
    char code1;
    char code2;
    for (char c : str) {
        if (c == ' ') {
            encodedString += '+';
        } else if (isalnum(c)) {
            encodedString += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            code2 = '\0';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
            // encodedString+=code2;
        }
    }
    return encodedString;
}
