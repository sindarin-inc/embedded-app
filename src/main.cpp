/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x64 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/


// Obvious BUGS
// (1) Prev doesn't work at all
// (2) When you turn it off and back on, if you hadn't gone forward before,
// it goes to the screen you were on, but if you had gone forward, it goes
// to the next screen
// (3) When you go back to the main menu from the text, and then go back
// to the text, it returns you to the NEXT page instead of the page you were
// on (similar to the above).
//

#include <Arduino.h>
#include <Preferences.h>

#include "SPIFFS.h"
#include "mbedtls/md.h"

#include "config.h"

#include "Contents/Readable.hpp"
#include "Displays/AllDisplays.hpp"
#include "Optics/AllOptics.hpp"
#include "Renderers/Renderer.hpp"
#include "Renderers/FileRenderer.hpp"
#include "Renderers/RawRenderer.hpp"
#include "Menus/MenuSystem.hpp"
#include "Inputs/RemoteInput.hpp"
#include "Contents/Json.hpp"
#include "Communications/PocketAPI.hpp"
#include "Communications/BLESettings.hpp"

#include "sindarin-debug.h"

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#ifdef TINYPICO_SINDARIN
#include <TinyPICO.h>
TinyPICO *tp;
#endif

#ifdef EINK154in
DisplayEink154in* my_display;
#endif

#ifdef OLED96in
DisplayOLED96in* my_display;
#endif

#ifdef SHARPMEM
DisplaySharpMem* my_display;
#endif

#ifdef PlanoConvex20x40
OpticsPlanoConvex20mmX40mm* my_optics;
#endif

#ifdef PlanoConvex16x36
OpticsPlanoConvex16mmX36mm* my_optics;
#endif

#ifdef Hastings20x40_3
OpticsHastings20mmX40_3mm* my_optics;
#endif

Renderer* box_renderer;
MenuSystem* main_menu;
#ifdef USE_IR_REMOTE
#include "Inputs/IRRemoteInput.hpp"
IRRemoteInput *ir_remote_input;
#endif
#ifdef USE_BLE_SERVER_REMOTE
#include "Inputs/BLERemoteInput.hpp"
#include "Communications/BLEWifi.hpp"
BLERemoteInput *ble_remote_input;
BLESettings *ble_settings;
// TODO(john): For now, grouping BLEWifi with BLERemoteInput since they're both
// BLE server services. These are separate things and it will be better to make
// them independent.
BLEWifi *ble_wifi;
#endif
#ifdef USE_TURN_TOUCH_REMOTE
#include "Inputs/TTRemoteInput.hpp"
TTRemoteInput *tt_remote_input;
#endif

#define IR_REMOTE_INPUT_PIN 12

// Whether to redisplay the box or not
int update_box = 1;

FileRenderer *book_renderer = nullptr;
// Raw renderer was put in place for BLE uploaded data test -cojo
RawRenderer *raw_renderer = nullptr;
unsigned long raw_place = 0, raw_place_prev = 0;

// Here are all the preferences that we will save off
// Eventually we should put this in an array so it's
// easier to save off, read them all in, and update
// it.
Preferences prefs;
int local_x_offset = 0;
int local_y_offset = 0;
int area_in_browsing = 0;
bool hasPartialRefreshed = false;

#define SETTINGS_MENU_TEXT "**SETTINGS**"

unsigned long new_place = 0;

void (*resetFunc)(void) = 0; //declare reset function @ address 0

int cur_menu_page = 0;
#define NUM_LINES_PER_MENU_PAGE 5

SpiRamJsonDocument *manifest = nullptr;
std::vector<Readable> readables;

void setup(void)
{
  if (Serial) Serial.begin(115200);
  SHOW_HEAP();

  DEBUG_LN("About to try drawing something!");

  // Read / Write the preferences
  prefs.begin("sindarin-app", false);
 
  // Only uncomment this if you need to reset the prefs.
  // Then recomment after loading onto board and restart.
  // prefs.clear();

  // Launch SPIFFS file system
  SHOW_HEAP();
  if (!SPIFFS.begin(false, "/spiffs", 255, NULL))
  {
    DEBUG_LN("An Error has occurred while mounting SPIFFS");
  }

  SHOW_HEAP();
  manifest = new SpiRamJsonDocument(2048);
  File manifestFile = SPIFFS.open("/manifest.json");
  if (!manifestFile)
  {
    DEBUG_LN("Failed to open manifest file.");
  }
  deserializeJson(*manifest, manifestFile);
  manifestFile.close();

  JsonArray array = manifest->as<JsonArray>();
  for (JsonVariant v : array)
  {
    readables.emplace_back(v["id"], v["name"], v["path"]);
  }
  SHOW_HEAP();

#ifdef TINYPICO_SINDARIN
  tp = new TinyPICO();
  SIZE_OF(*tp);
#endif

#ifdef EINK154in
  my_display = new DisplayEink154in();
  SIZE_OF(*my_display);
  //my_display->set_font(u8g2_font_6x10_tf);
#endif

#ifdef OLED96in
  my_display = new DisplayOLED96in();
#endif

#ifdef SHARPMEM
  my_display = new DisplaySharpMem();
#endif

#ifdef PlanoConvex20x40
  my_optics = new OpticsPlanoConvex20mmX40mm();
#endif
  
#ifdef PlanoConvex16x36
  my_optics = new OpticsPlanoConvex16mmX36mm();
#endif

#ifdef Hastings20x40_3
  my_optics = new OpticsHastings20mmX40_3mm();
  SIZE_OF(*my_optics);
#endif

  SHOW_HEAP();

  box_renderer = new Renderer(my_optics, my_display);
  SIZE_OF(*box_renderer);

  //my_display->put_up_hello_world();
  #ifdef USE_IR_REMOTE
    ir_remote_input = new IRRemoteInput();
    SIZE_OF(*ir_remote_input);
    ir_remote_input->start_remote(IR_REMOTE_INPUT_PIN);
  #endif

  #ifdef USE_BLE_SERVER_REMOTE
    ble_remote_input = new BLERemoteInput();
    SIZE_OF(*ble_remote_input);
    ble_remote_input->start_remote();
    ble_settings = new BLESettings(&prefs);
    ble_settings->listen_for_settings();

    // TEST: Setup receiver for new data via BLE
    BLECharacteristic *newDataCharacteristic = BLEManager::getInstance()->service->createCharacteristic(
        BLEUUID("6851fc86-9284-4ff9-9ea8-82e22bc2f1b1"),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

    class DataCharacteristicCallbacks : public BLECharacteristicCallbacks
    {
    public:
      RawRenderer **renderer = nullptr;
      int *update_flag;
      DataCharacteristicCallbacks(RawRenderer **render, int *update_box) : renderer(render), update_flag(update_box) {}

      void onWrite(BLECharacteristic *characteristic)
      {
        std::string value = characteristic->getValue();
        *renderer = new RawRenderer(my_optics, my_display, value);
        SIZE_OF(**renderer);
        *update_flag = 1;
        if (value.length() > 0)
        {
          DEBUG_LN("*********");
          DEBUG_F("New value: %s \n", value.c_str());
          DEBUG_LN("*********");
        }
      }
    };

    newDataCharacteristic->setCallbacks(new DataCharacteristicCallbacks(&raw_renderer, &update_box));
    // END TEST code

    ble_wifi = new BLEWifi();
    SIZE_OF(*ble_wifi);
    ble_wifi->startService();

    BLEManager::getInstance()->startServiceAndAdvertising();

  #endif

  // Load the preferences
  //
  //my_display->draw_text("testing testing hi there!\ntwo lines\nthreelines\n\n\n\nskipthreelines\n", 0, 0);
  my_display->display_splash_screen();


  delay(500);
  // ugly way to clear the screen
  local_x_offset = prefs.getInt("x-offset");
  local_y_offset = prefs.getInt("y-offset");
  // Default area for browsing should be MAIN MENU which is readables length + 2 (length is SETTINGS MENU)
  area_in_browsing = prefs.getInt("browse-area", readables.size() + 2);

  DEBUG_VAR(area_in_browsing);
  DEBUG_LN(" was the area read in");


  main_menu = new MenuSystem();
  SIZE_OF(*main_menu); // 1208

  SHOW_HEAP();
  for (Readable &r : readables)
  {
    String name = String(r.id) + "-p";
    String name_prev = String(r.id) + "-pr-p";
    main_menu->add_menu_item(r.name);
    r.place = prefs.getULong(name.c_str());
    r.place_prev = prefs.getULong(name_prev.c_str());
    SHOW_HEAP();
  }

  if (area_in_browsing < readables.size())
  {
    book_renderer = new FileRenderer(my_optics, my_display, readables[area_in_browsing]);
    SIZE_OF(*book_renderer);
  }

  main_menu->add_menu_item("Pocket Articles");
  main_menu->add_menu_item(SETTINGS_MENU_TEXT);
 
  //box_renderer->render_max_box_with_text(local_x_offset,local_y_offset, "The Lord is my shepherd\n");
  box_renderer->render_max_box(local_x_offset, local_y_offset);
  delay(250);
  update_box = 1;
    
  //box_renderer->render_max_box(local_x_offset, local_y_offset);
  my_display->set_partial_refresh();

  SHOW_HEAP();

  //box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, main_menu->get_whole_menu_as_string();
}

const int MAX_ANALOG_VAL = 4095;
const float MAX_BATTERY_VOLTAGE = 4.2; // Max LiPoly voltage of a 3.7 battery is 4.2

String get_battery_life() {
#ifdef TINYPICO_SINDARIN
  float voltageLevel = tp->GetBatteryVoltage();
#else
  // A13 pin is not exposed on Huzzah32 board because it's tied to
  // measuring voltage level of battery. Note: you must
  // multiply the analogRead value by 2x to get the true battery
  // level. See:
  // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/esp32-faq
  int rawValue = analogRead(A13);

  // Reference voltage on ESP32 is 1.1V
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html#adc-calibration
  // See also: https://bit.ly/2zFzfMT
  float voltageLevel = (rawValue / 4095.0) * 2 * 1.1 * 3.3; // calculate voltage level
#endif
  float batteryFraction = voltageLevel / MAX_BATTERY_VOLTAGE;
  String battery_output = "  Percent: " + String(batteryFraction * 100) + "%";
  DEBUG_LN(battery_output.c_str());
  SHOW_HEAP();
  return battery_output;
}

void loop(void) {
  //int i;
  String menu_string;
  String bat_life;
  String total_to_display;
  //delay(2000);
  //for (i = 0; i < 60; i+=40) {
  
  //my_display->draw_box(34, 12, 94, 52);
  //delay(10000);
  //FileRenderer* current_render = 0;

  #ifdef USE_BLE_SERVER_REMOTE
    ble_wifi->loop();
  #endif

  if (update_box)
  {
    // box_renderer->render_max_box(local_x_offset, local_y_offset);
    DEBUG_LN("UPDATING THE BOX!");

    // MAIN MENU is readables length + 1 (length is SETTINGS MENU)
    if (raw_renderer != nullptr)
    {
      if (raw_renderer != nullptr && raw_renderer->get_read_mode() == PAGE_READ)
      {
        DEBUG_LN(" Rendering some remote text in READ MODE");
        new_place = raw_renderer->render_page_with_box(raw_place, local_x_offset, local_y_offset);
        DEBUG_VAR(new_place);
        DEBUG_LN(" is the new place in the text");
      }
    }
    else if (area_in_browsing == readables.size() + 1)
    {
      menu_string = main_menu->get_menu_range_as_string(cur_menu_page * NUM_LINES_PER_MENU_PAGE + 1, cur_menu_page * NUM_LINES_PER_MENU_PAGE + 5);
      DEBUG_LN("UPDATING THE MENU WITH CURR MENU_PAGE: ");
      DEBUG_VAR(cur_menu_page);
      DEBUG_LN(" is the current menu page");
      bat_life = get_battery_life();
      total_to_display = menu_string + String("\n\n") + bat_life;
      box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, total_to_display);
      // Partial refresh doesn't seem to work on both eInk displays on very first render, only hits one? -cojo
      if (!hasPartialRefreshed)
      {
        hasPartialRefreshed = true;
        box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, total_to_display);
      }
    }
    else if (area_in_browsing == readables.size()) // Pocket API menu
    {
      if (!WiFi.isConnected()) {
        box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, String("WiFi Disconnected"));
      } else {
        // TODO: This is a proof-of-concept. To implement this for real, we'll probably want
        // to implement loading a submenu of all your articles, but that should probably be
        // a part of a larger menu refactor.
        log_i("Loading top Pocket article");
        box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, String("Downloading..."));
        PocketAPI *pocket = PocketAPI::getInstance();
        // TODO: This will come from the phone
        char access_token[40];
        prefs.getString(POCKET_API_TOKEN_PREFS_KEY, access_token, sizeof(access_token));
        pocket->setAccessToken(access_token);
        const char *text;
        auto code = pocket->getFirstArticleText(&text);
        if (code == 200) {
          auto str = std::string(text);
          raw_renderer = new RawRenderer(my_optics, my_display, str);
          raw_renderer->render_page_with_box(raw_place, local_x_offset, local_y_offset);
        } else {
          box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, String("Pocket Error: ") + code);
        }
      }
    }
    else if (area_in_browsing == readables.size() + 1)
    {
      menu_string = "Press arrows to move\nbox around to your\nliking. Then press\nenter when done!\nHave a nice day.\n";
      bat_life = get_battery_life();
      total_to_display = menu_string + String("\n\n") + bat_life;
      box_renderer->render_max_box_with_text(local_x_offset, local_y_offset, total_to_display);
    }
    else
    {
      // All the contents are the rest of the menu items
      if (book_renderer != nullptr && book_renderer->get_read_mode() == PAGE_READ)
      {
        DEBUG_LN(" Rendering some text in READ MODE");
        new_place = book_renderer->render_page_with_box(readables[area_in_browsing].place, local_x_offset, local_y_offset);
        DEBUG_VAR(new_place);
        DEBUG_LN(" is the new place in the text");
      }
    }
    //my_display->draw_box(start_x, start_y, end_x, end_y);
    update_box = 0;
  }

  if (raw_renderer != nullptr)
    raw_renderer->render_continue_screed(local_x_offset, local_x_offset);
  else if (area_in_browsing < readables.size())
    readables[area_in_browsing].place_screed = book_renderer->render_continue_screed(local_x_offset, local_y_offset);

  // Always have to do a check press and check release. In this case, we don't care about press.
  #ifdef USE_IR_REMOTE
    ir_remote_input->check_press();
  #endif
  #ifdef USE_BLE_SERVER_REMOTE
    ble_remote_input->check_press();
  #endif
  #ifdef USE_TURN_TOUCH_REMOTE
    if (tt_remote_input == nullptr)
    {
      tt_remote_input = new TTRemoteInput();
      tt_remote_input->start_remote();
    }
    tt_remote_input->check_press();
  #endif

  // We could do this better by iterating, but fine for now with only 2 input devices.
  // Note that if an IR input comes in and then a BLE input comes at the same time,
  // it may only get one of them. This seems unlikely.
  int got_a_release = 0;
  int code_from_release = 0;

  #ifdef USE_IR_REMOTE
    if (ir_remote_input->check_release()) {
      got_a_release = 1;
      code_from_release = ir_remote_input->get_code();
    }
  #endif

  #ifdef USE_BLE_SERVER_REMOTE
    if (ble_remote_input->check_release()) {
      got_a_release = 1;
      code_from_release = ble_remote_input->get_code();
    }
  #endif

  #ifdef USE_TURN_TOUCH_REMOTE
    if (tt_remote_input->check_release())
    {
      got_a_release = 1;
      code_from_release = tt_remote_input->get_code();
    }
  #endif

  auto select = []()
  {
    // Check against readables.size() because it is == SETTINGS_MENU
    if (area_in_browsing == readables.size())
    {
      prefs.putInt("x-offset", local_x_offset);
      prefs.putInt("y-offset", local_y_offset);
      DEBUG_LN("Saving off x and y offset to preferences");
      DEBUG_VAR(local_x_offset);
      DEBUG_LN(" is the x offset");
      DEBUG_VAR(local_y_offset);
      DEBUG_LN(" is the y offset");
      // Set to MAIN_MENU (readables.size() + 1)
      area_in_browsing = readables.size() + 1;
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;
    }
    else if (area_in_browsing == readables.size() + 1)
    {
      // CAN FIX THIS NOW TO JUST DO ONE ASSIGNMENT BASED ON THE
      // RETURN RESULT OF THE MENU!
      area_in_browsing = main_menu->get_curr_selection();
      if (book_renderer != nullptr)
      {
        delete book_renderer;
        book_renderer = nullptr;
      }
      // Only create the FileRenderer if we didn't select the SETTINGS MENU
      if (area_in_browsing != readables.size()) {
        book_renderer = new FileRenderer(my_optics, my_display, readables[area_in_browsing]);
        SIZE_OF(*book_renderer);
      }
      prefs.putInt("browse-area", area_in_browsing);
      update_box = 1;
    }
  };

  auto back = []()
  {
    if (book_renderer != nullptr && book_renderer->get_read_mode() == SCREED_READ)
    {
      String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
      String prefs_name = String(readables[area_in_browsing].id) + "-p";
      readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
      readables[area_in_browsing].place = readables[area_in_browsing].place_screed;
      prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
      prefs.putULong(prefs_prev_name.c_str(), readables[area_in_browsing].place_prev);
    }

    // Set to MAIN_MENU (readables.size() + 1)
    area_in_browsing = readables.size() + 1;
    prefs.putInt("browse-area", area_in_browsing);
    update_box = 1;
    if (book_renderer != nullptr)
    {
      delete book_renderer;
      book_renderer = nullptr;
    }
    if (raw_renderer != nullptr)
    {
      delete raw_renderer;
      raw_renderer = nullptr;
      raw_place = 0;
    }
  };

  if (got_a_release)
  {
    switch(code_from_release) {
       case SETUP:
          prefs.clear();
          DEBUG_LN("CLEARED PREFERENCES!");
          DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
          resetFunc();
          break;
        case STOP:
          DEBUG_LN("RESETTING BOARD BY SEG FAULT!");
          resetFunc();
          break;
        case CHANNEL_MINUS:
          // TODO: Check bounds
          //if (start_x != 0) {
          if (raw_renderer != nullptr)
          {
            raw_place = raw_place_prev;
          }
          // Check against readables.size() because it is == SETTINGS_MENU
          else if (area_in_browsing == readables.size())
            local_x_offset--;
          else if (area_in_browsing < readables.size())
          {
            String prefs_name = String(readables[area_in_browsing].id) + "-pr-p";
            readables[area_in_browsing].place = book_renderer->get_previous_place();
            prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
          }
          update_box = 1;
          //}
          break;
        case CHANNEL_PLUS:
          // TODO:
          // Check to be sure we don't go off the end of the screen
          //if (end_x != my_display->get_x()) {
          if (raw_renderer != nullptr)
          {
            raw_place_prev = raw_place;
            raw_place = new_place;
          }
          // Check against readables.size() because it is == SETTINGS_MENU
          else if (area_in_browsing == readables.size())
            local_x_offset++;
          else if (area_in_browsing < readables.size())
          {
            String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
            String prefs_name = String(readables[area_in_browsing].id) + "-p";

            readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
            readables[area_in_browsing].place = new_place;
            prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
            prefs.putULong(prefs_prev_name.c_str(), readables[area_in_browsing].place_prev);
          }
          update_box = 1;
          //}
          select();
          break;
        case PREV:
          // TODO: Check bounds
          //if (start_y != 0) {
          if (raw_renderer != nullptr)
          {
            if (raw_renderer->get_read_mode() == SCREED_READ)
              raw_renderer->increase_speed();
          }
          // Check against readables.size() because it is == SETTINGS_MENU
          else if (area_in_browsing == readables.size())
            local_y_offset--;
          // Check against readables.size() + 1 because it is == MAIN_MENU
          else if (area_in_browsing == readables.size() + 1)
          {
            if (main_menu->prev_selection()) {
              // Hit a page boundary -- update the page!
                if ((main_menu->get_curr_selection() % NUM_LINES_PER_MENU_PAGE) == NUM_LINES_PER_MENU_PAGE - 1) {
                  cur_menu_page--;
                }
            }
          }
          else {
            if (book_renderer->get_read_mode() == SCREED_READ)
              book_renderer->increase_speed();
          }
          update_box = 1;
          //}
          back();
          break;
        case NEXT:
          // TODO: Check to be sure we don't go off the end of the screen
          // if (end_y != my_display->get_y()) {
          if (raw_renderer != nullptr)
          {
            if (raw_renderer->get_read_mode() == SCREED_READ)
              raw_renderer->decrease_speed();
          }
          // Check against readables.size() because it is == SETTINGS_MENU
          else if (area_in_browsing == readables.size())
            local_y_offset++;
          // Check against readables.size() + 1 because it is == MAIN_MENU
          else if (area_in_browsing == readables.size() + 1)
          {
            if (main_menu->next_selection()) {
              // Hit a page boundary -- update the page!
              if (main_menu->get_curr_selection() % NUM_LINES_PER_MENU_PAGE == 0) {
                  cur_menu_page++;
              }
            }
          }
          else {
            if (book_renderer->get_read_mode() == SCREED_READ)
              book_renderer->decrease_speed();
          }
          update_box = 1;
          //}
          break;
        case ENTER:
          select();
          break;
        case BACK:
          back();
          break;
        case NOP_BUTTON:
          if (raw_renderer != nullptr)
          {
            raw_renderer->flip_read_mode();
          }
          else if (area_in_browsing < readables.size())
          {
            if (book_renderer->get_read_mode() == SCREED_READ)
            {
              String prefs_prev_name = String(readables[area_in_browsing].id) + "-pr-p";
              String prefs_name = String(readables[area_in_browsing].id) + "-p";
              readables[area_in_browsing].place_prev = readables[area_in_browsing].place;
              readables[area_in_browsing].place = readables[area_in_browsing].place_screed;
              prefs.putULong(prefs_name.c_str(), readables[area_in_browsing].place);
              prefs.putULong(prefs_prev_name.c_str(), readables[area_in_browsing].place_prev);
            }
            book_renderer->flip_read_mode();
          }
          update_box = 1;
        break;
        default:
        break;
        }
  }
}