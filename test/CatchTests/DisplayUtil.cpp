#include "DisplayUtil.hpp"

extern Font fontFace0;
extern Font fontFace1;
extern Font fontFace2;

std::shared_ptr<DisplayEinkET013TT1> createDisplay() {
    auto display = std::make_shared<DisplayEinkET013TT1>(0, 0, 0);
    (*display).setSkipWaveform(true);
    (*display).setSkipSignalling(true);
    (*display).setFont(static_cast<Font *>(&fontFace1));
    (*display).setLineSpacing(0);
    (*display).setup();
    (*display).invertDisplay(false);
    return display;
}
