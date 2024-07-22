The following changes have been made to the application in support of the added TTF fonts:

Principles:

- Only one type of font, TTF or IBMF is possible in the application
- Only one font is being use by the application in three font sizes
- GFX fonts no longer in use
- Three fonts are defined for each type: fontFace0 (small), fontFace1 (medium) and fontFace2 (large). 
  Those are the names to be used everywhere in the application source code.
- class names for IBMFFont and TTFFont changed to Font, removing the need for IBMF/TTF related naming 
  conditionals in the rest of the application source code
- Some code refactoring to get rid of some unsused code and some class renames to better suitable names

Changes:

- config.h
    - Added CONFIG_FONT_IBMF and CONFIG_FONT_TTF defines. Some error checks have also been added
    - Added a "#pragma once" line at the beginning

- all source file that were using ibmf75FaceX (X = 0, 1 or 2) have been updated to use fontFaceX instead
- class IBMFFontLow renamed to FontData as it relates to the binary .h font information present in the IBMFFonts folder. Source files renamed from IBMFFontLow.(hpp,cpp) to IBMFFontData.(hpp, cpp)
- class IBMFFont renamed to Font. Source files remains to be IBMFFont.(hpp, cpp). TTFFont is also named Font, removing the need to have conditional compilation in all the app.
- the UTF8Iterator (file UI/Fonts/IBMFDriver/UTF8Iterator.hpp) class has been moved from the IBMFDriver folder to the Fonts folder, as it is going to be used by both the IBMF and TTF Drivers
- A new UI/Fonts/FontDefs.hpp containing common structure definitions used by both Drivers
- A new UI/Fonts/Font.hpp that is usable by the application to retrived proper definitions to use the Drivers
- platformio.ini modified to 
  - add the linux's freetype library for the 8-bits simulator. There coud be issues related to the test automations on GitHub that could require some adjustments.
  - add CONFIG_FONT_IBMF and CONFIG_FONT_TTF -D definitions for both simulators (current Sol and 8-bits)

- all GFX Fonts removed (folder src/UI/Fonts/GFXFonts deleted)
- all use of GFX fonts removed (mainly in the DisplaySystem class)

What remains to be done for first cut (The current code compile but still not usable):

- Document how the FreeType library was built
- Some codification related to FreeType in TTFFont 
- Merge of the main branch to the guy/ttf-support branch
- Debugging 

Future steps after first cut:

- I will need to know which TTF font will be used with the upcoming Sol version. There maybe some changes required to that font, mainly removing the characters that won't be useful if it is to large for integration in the Sol app.
- From this knowledge, a complementary font will need to be designed to contain the supplemental glyphs used by the Sol glasses. Made using the FontForge application, some of the characters will be retrieved from the main font and adapted for their use as subscripts/superscripts. The Sol Icon and the UnknownCharacter Icon (Question mark inside a rounded box) will also be added.
- The code in the font driver will require some modifications to add the complementary font to the process.

- Some options to complete (e.g. inverse video, ...)
- Add kerning/ligature support
- First Code Optimization (without caching)
- Second Code Optimization (with caching) if required
- Some code refactoring if required
- Debugging (Using the 8-bits simulator)
- automated tests under github
  - make compilation work for 8-bits mode
  - test cases
- Tests with the Sol glasses for the current version