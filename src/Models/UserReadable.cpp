#include "UserReadable.hpp"

bool convertToJson(const UserReadable &src, JsonVariant dst) {
    dst["id"] = src.id;
    dst["readable"] = src.readable;

    return true;
}
