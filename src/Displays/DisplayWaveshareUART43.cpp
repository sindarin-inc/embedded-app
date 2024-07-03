#include "Displays/DisplayWaveshareUART43.hpp"

#include "Communications/USBSerial.hpp"
#include "Misc/Platform.hpp"
#include "config.h"

extern bool mirrorToSerial;

/*******************************************************************************
 * Function Name  : void base_draw(void)
 * Description    :
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
void DisplayWaveshareUART43::base_draw(void) {
    int i, j;

    /*
    draw pixel
    */
    /*
     driver_.epd_clear();
     for (j = 0; j < 600; j += 50) {
         for (i = 0; i < 800; i += 50) {
             driver_.epd_draw_pixel(i, j);
             driver_.epd_draw_pixel(i, j + 1);
             driver_.epd_draw_pixel(i + 1, j);
             driver_.epd_draw_pixel(i + 1, j + 1);
         }
     }
     driver_.epd_udpate();
     Delay(2000);
 */
    /*
    draw line
    */
    /*
     driver_.epd_clear();
     for (i = 0; i < 800; i += 100) {
         driver_.epd_draw_line(0, 0, i, 599);
         driver_.epd_draw_line(799, 0, i, 599);
     }
     driver_.epd_udpate();
     Delay(2000);
 */
    /*
    fill rect
    */
    driver_.epd_clear();
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_fill_rect(10, 10, 100, 100);

    driver_.epd_set_color(DARK_GRAY, WHITE);
    driver_.epd_fill_rect(110, 10, 200, 100);

    driver_.epd_set_color(GRAY, WHITE);
    driver_.epd_fill_rect(210, 10, 300, 100);

    driver_.epd_udpate();
    Delay(2000);

    /*
    draw circle
    */
    /*
     driver_.epd_set_color(BLACK, WHITE);
     driver_.epd_clear();
     for (i = 0; i < 300; i += 40) {
         driver_.epd_draw_circle(399, 299, i);
     }
     driver_.epd_udpate();
     Delay(2000);
 */
    /*
    fill circle
    */
    /*
     driver_.epd_clear();
     for (j = 0; j < 6; j++) {
         for (i = 0; i < 8; i++) {
             driver_.epd_fill_circle(50 + i * 100, 50 + j * 100, 50);
         }
     }
     driver_.epd_udpate();
     Delay(2000);
 */
    /*
    draw triangle
    */
    driver_.epd_clear();
    for (i = 1; i < 5; i++) {
        driver_.epd_draw_triangle(399, 249 - i * 50, 349 - i * 50, 349 + i * 50, 449 + i * 50,
                                  349 + i * 50);
    }
    driver_.epd_udpate();
    Delay(3000);
}

void DisplayWaveshareUART43::draw_text_demo(void) {
    char buff[] = {'G',  'B',  'K',  '3',  '2',  ':',  ' ',  0xc4,
                   0xe3, 0xba, 0xc3, 0xca, 0xc0, 0xbd, 0xe7, 0};
    driver_.epd_set_color(BLACK, WHITE);
    driver_.epd_clear();
    driver_.epd_set_ch_font(GBK32);
    driver_.epd_set_en_font(ASCII32);
    driver_.epd_disp_string(buff, 0, 50);
    driver_.epd_disp_string("ASCII32: Hello, World!", 0, 300);

    driver_.epd_set_ch_font(GBK48);
    driver_.epd_set_en_font(ASCII48);
    buff[3] = '4';
    buff[4] = '8';
    driver_.epd_disp_string(buff, 0, 100);
    driver_.epd_disp_string("ASCII48: Hello, World!", 0, 350);

    driver_.epd_set_ch_font(GBK64);
    driver_.epd_set_en_font(ASCII64);
    buff[3] = '6';
    buff[4] = '4';
    driver_.epd_disp_string(buff, 0, 160);
    driver_.epd_disp_string("ASCII64: Hello, World!", 0, 450);

    driver_.epd_udpate();
    Delay(2000);
}

auto DisplayWaveshareUART43::setup() -> bool {

    DisplaySystem::setup();
    driver_.epd_init();
    driver_.epd_wakeup();
    return true;
}

void DisplayWaveshareUART43::updateDisplay(DisplaySystem::UpdateType updateType,
                                           bool clearFrameBuffer, int vcomNudge,
                                           DisplaySystem::UpdateType altUpdateType,
                                           ScreenSide screen, Rect altRect) {
    if (mirrorToSerial) {
        log_i("Implement a new way to mirror to serial since we don't have a framebuffer");
        // MAGIC NUMBER FIX ME!
        // USBSerial::getInstance().txSendData(
        //     reinterpret_cast<const uint8_t *>(gfxCanvas_.getBuffer()), 8192);
    }

    if (updateType == DisplaySystem::UpdateType::FULL) {
        // base_draw();
        log_i("attempting text demo");
        draw_text_demo();
    }
}