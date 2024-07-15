#include "DisplayUtil.hpp"

std::shared_ptr<DisplayEinkET013TT1> createDisplay() {
    auto display = std::make_shared<DisplayEinkET013TT1>(0, 0, 0);
    (*display).setSkipWaveform(true);
    (*display).setSkipSignalling(true);
    (*display).style.setFontFamily((*display).style.solSansMedium);
    (*display).setLineSpacing(0);
    (*display).setup();
    (*display).invertDisplay(false);
    return display;
}
