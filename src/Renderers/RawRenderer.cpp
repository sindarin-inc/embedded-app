#include "RawRenderer.hpp"

#include "sindarin-debug.h"

void RawRenderer::setString(std::string &string) {
    this->stream_ = std::istringstream(string);
    this->currentPlace_ = 0;
    this->previousPlace_ = 0;

    // LOAD TEXT for prev, current.
    this->loadPrevIntoBuffer();
    this->loadCurrIntoBuffer();
}
