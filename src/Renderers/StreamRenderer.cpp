#include "StreamRenderer.hpp"

#include "sindarin-debug.h"

void StreamRenderer::setString(std::string &string) {
    auto stream = std::stringstream(string);
    setStream(stream);
}

void StreamRenderer::setStream(std::stringstream &stream) {
    this->stream_ = std::move(stream);
    this->currentPlace_ = 0;
    this->previousPlace_ = 0;

    // LOAD TEXT for prev, current.
    this->loadPrevIntoBuffer();
    this->loadCurrIntoBuffer();
}