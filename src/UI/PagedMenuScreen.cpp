#include "PagedMenuScreen.hpp"

#include <Adafruit_GFX.h>
#include <string>

#include "config.h"

#include "Bitmaps/Menu_Blank.hpp"
#include "Displays/DisplayEinkET013TT1.hpp"
#include "Fonts/LucidaGrande10pt7b.h"
#include "Fonts/LucidaGrande14pt7b.h"
#include "Fonts/LucidaGrande5pt7b.h"
#include "Fonts/LucidaGrande6pt7b.h"

#define MAX_WIDTH 152

// BC: I'm reducing the number of fonts to 2 here as I feel 14 is too big and 5 is too
// small when I look at it in glass
const GFXfont *shrinkingFonts[] = {/*&LucidaGrande14pt7b,*/
                                   &LucidaGrande10pt7b, &LucidaGrande6pt7b
                                   /*,&LucidaGrande5pt7b */};

void PagedMenuScreen::render() {
    if (!needs_rerender) {
        return;
    }
    log_i("PagedMenuScreen render (selected = %s)", get_curr_item().title.c_str());
#ifdef EINKET013TT1
    DisplayEinkET013TT1 *eink = (DisplayEinkET013TT1 *)renderer.get_display();
    if (eink != nullptr) {
        auto autoUpdate = eink->get_auto_update();
        eink->set_auto_update(false);
        // Blank out screen
        eink->fillScreen(eink->bg);
        // Draw menu surroundings
        eink->drawXBitmap(0, 0, Menu_Blank_bits, 256, 256, eink->fg);
        String itemTitle = get_curr_item().title;
        // Cache / replace old font; then try descending size fonts until we find one that fits our
        // string in the desired space.
        auto prevFont = eink->get_font();
        for (int i = 0; i < sizeof(shrinkingFonts); ++i) {
            eink->set_font(shrinkingFonts[i]);
            if (eink->get_text_width(itemTitle.c_str()) < MAX_WIDTH) {
                break;
            }
            log_d("Font size %d was too big for menu item title; stepping down!", i);
        }
        // If we still don't fit, shrink the string until we do.
        while (eink->get_text_width(itemTitle.c_str()) > MAX_WIDTH) {
            log_d("Item title %s still too long at smallest font size; truncating...",
                  itemTitle.c_str());
            itemTitle = itemTitle.substring(0, itemTitle.length() - 4) + "...";
        }
        // Now draw!
        uint16_t width = 0, height = 0;
        String battery_status = this->power.getBatteryLifeString();
        eink->get_text_bounds(itemTitle.c_str(), &width, &height);
        eink->draw_text(itemTitle.c_str(),
                        this->renderer.local_x_offset + (EinkET013TT1_X_PIXELS) / 2 - (width / 2),
                        this->renderer.local_y_offset + (EinkET013TT1_Y_PIXELS) / 2 + (height / 2));
        eink->setFont(&LucidaGrande5pt7b);
        eink->get_text_bounds(battery_status.c_str(), &width, &height);
        // Arbitrarily put the battery text off of the bottom of the screen 20 pixels
        // This is just so we always have it in our prototype, not that it'll be
        // the right place (or icon) long term
        // Not taking into account the rednerer x and y offsets here on purpose
        eink->draw_text(battery_status.c_str(), (EinkET013TT1_X_PIXELS) / 2 - (width / 2),
                        EinkET013TT1_Y_PIXELS - 20);
        eink->updateDisplay();
        eink->setFont(prevFont);
        eink->set_auto_update(autoUpdate);
    }
#endif
    needs_rerender = false;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::next_selection() -> int {
    curr_selection = (curr_selection + 1) % num_menu_items;
    return num_menu_items > 1 ? 1 : 0;
}

// Returns 1 if the selection was updated, 0 otherwise
auto PagedMenuScreen::prev_selection() -> int {
    curr_selection = (curr_selection - 1) % num_menu_items;
    return num_menu_items > 1 ? 1 : 0;
}