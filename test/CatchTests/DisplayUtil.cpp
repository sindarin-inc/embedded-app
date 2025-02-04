#include "DisplayUtil.hpp"

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

std::shared_ptr<DisplayEinkET013TT1> createDisplay() {
    auto display = std::make_shared<DisplayEinkET013TT1>();
    (*display).setSkipWaveform(true);
    (*display).setSkipSignalling(true);
    (*display).setup();
    (*display).invertDisplay(false);
    return display;
}
