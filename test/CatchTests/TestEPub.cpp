#include <Defer/Defer.hpp>
#include <catch_amalgamated.hpp>

// Allow access to protected methods from tests
#define protected public
#define private public
#include "Misc/BMPImage.hpp"
#include "Misc/Telemetry.hpp"
#include "Renderers/RendererDefs.hpp"
#include "Renderers/RendererStream.hpp"
#include "Storage/PosixFileSystem.hpp"
#include "UI/BitmapScreen.hpp"
#include "UI/LayoutTestScreen.hpp"
#include "UI/LineSpacingScreen.hpp"
#include "UI/OffsetAdjustScreen.hpp"
#include "UI/PowerDepletedScreen.hpp"
#include "UI/ReadableScreen.hpp"
#include "UI/ReadableTableOfContentsScreen.hpp"
#include "UI/ResizeBoxScreen.hpp"
#include "UI/Screen.hpp"
#include "ViewTestCommon.hpp"
#undef protected
#undef private

#include <sstream>

extern auto UIGetCurrentScreen() -> std::shared_ptr<Screen>;

TEST_CASE("Test some individual EPub formatting edge cases", "[epub-format]") {
    SetImagesFolder("EPubImages");

    auto display = createDisplay();

    PosixFileSystem fs("./test/CatchTests");
    fs.begin();

    SECTION("Test that newlines without spaces are rendered as spaces") {
        auto readable = std::make_shared<Readable>();
        readable->readableType = "book";

        auto file = fs.open("/Fixtures/epub/formatting.epub");
        auto prefs = PreferencesStore();
        SolDrm solDrm(prefs);
        auto epubFile = std::make_shared<EPubFile>(file, readable, solDrm);

        Renderer renderer(*display, display->defaultTextRect(), static_cast<Font *>(&fontFace0));
        renderer.setStream(epubFile);
        renderer.renderPage(std::make_shared<EPubPlace>(0, "newlines_without_spaces.xhtml"));
        display->updateDisplay(DisplaySystem::UpdateType::FULL, true);

        ASSERT_VIEW_UNCHANGED(display, "NewLineWithoutSpace");
    }

    SECTION("Test that >4 non-breaking space characters get collapsed") {
        auto readable = std::make_shared<Readable>();
        readable->readableType = "book";

        auto file = fs.open("/Fixtures/epub/formatting.epub");
        auto prefs = PreferencesStore();
        SolDrm solDrm(prefs);
        auto epubFile = std::make_shared<EPubFile>(file, readable, solDrm);

        Renderer renderer(*display, display->defaultTextRect(), static_cast<Font *>(&fontFace0));
        renderer.setStream(epubFile);
        renderer.renderPage(std::make_shared<EPubPlace>(0, "multiple_nbsp.xhtml"));
        display->updateDisplay(DisplaySystem::UpdateType::FULL, true);

        ASSERT_VIEW_UNCHANGED(display, "CollapsedNBPS");
    }

    SECTION("Test that all glyphs render inside the EPUB") {

        auto readable = std::make_shared<Readable>();
        readable->readableType = "book";

        auto file = fs.open("/Fixtures/epub/glyphs.epub");
        auto prefs = PreferencesStore();
        SolDrm solDrm(prefs);
        auto epubFile = std::make_shared<EPubFile>(file, readable, solDrm);

        Renderer renderer(*display, display->defaultTextRect(), static_cast<Font *>(&fontFace0));
        renderer.setStream(epubFile);
        auto place = std::make_shared<EPubPlace>(0, "Basic Latin.xhtml");

        // Render all pages
        int i = 0;
        while (!renderer.atEndOfFile()) {
            auto chapter = epubFile->getToc()[epubFile->getChapterIdx(place)];
            auto [newPlace, _] = renderer.renderPage(place);
            place = std::dynamic_pointer_cast<EPubPlace>(newPlace);
            display->updateDisplay(DisplaySystem::UpdateType::FULL, true);
            ASSERT_VIEW_UNCHANGED(display, "EPubGlyphTest/" + std::to_string(i) + "-" +
                                               std::string(chapter.title));
            i++;
        }
    }
}

TEST_CASE("Test EPub Books Rendering", "[.][epub]") {

    SetupFileSystemMock("EPubTestData");
    SetImagesFolder("EPubImages");

    StartAppMain(FONT_CHOICES::PREFS_SOL_SMALL, 10, 10, -2);
    defer { StopFreeRtos(); };

    auto &display = GetRootDisplay();

    // First screen renders twice once with a full refresh and once with a AU4
    WAIT_FOR_DRAW(display);
    WAIT_FOR_DRAW(display, false);

    ASSERT_VIEW_UNCHANGED(display, "FirstScreen");

    SECTION("Books") {
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display, false);

        ASSERT_VIEW_UNCHANGED(display, "OnePageTutorial");
        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display, false);

        auto screen = UIGetCurrentScreen();
        auto readableScreen = std::dynamic_pointer_cast<ReadableScreen>(screen);

        if (readableScreen != nullptr) {

            int pageNbr = 1;
            ChapterIdx chapterIdx = 0;
            auto epubFile = std::dynamic_pointer_cast<EPubFile>(readableScreen->getStream());

            if (epubFile != nullptr) {
                for (;;) {
                    StreamPlace streamPlace = readableScreen->getStreamPlace();
                    ChapterIdx idx = epubFile->getChapterIdx(streamPlace.getSpineIdx());
                    if (idx != chapterIdx) {
                        chapterIdx = idx;
                        pageNbr = 1;
                    }

                    std::stringstream fname;
                    fname << chapterIdx + 1 << "-" << pageNbr++;

                    ASSERT_VIEW_UNCHANGED(display, fname.str());

                    if (readableScreen->checkAtEnd()) break;

                    SendRemoteCommand(ENTER);
                    WAIT_FOR_DRAW(display, false);
                }
            }

        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book1.");
            CHECK(false);
        }

        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);

        ASSERT_VIEW_UNCHANGED(display, "LastPagePopupMenu");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);

        ASSERT_VIEW_UNCHANGED(display, "MainMenu");

#if 0
        // The following is kept as it would be useful if other books are integrated to the test.
        // }

        // SECTION("Book 2") {

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display, false);

        screen = UIGetCurrentScreen();
        readableScreen = std::dynamic_pointer_cast<ReadableScreen>(screen);

        if (readableScreen != nullptr) {

            // ASSERT_VIEW_UNCHANGED(display, "OnePageTutorial");
            // SendRemoteCommand(ENTER);
            // WAIT_FOR_DRAW(display);

            int pageNbr = 1;
            int spineIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getSpineIdx() != spineIdx) {
                    spineIdx = streamPlace.getSpineIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book2/" << spineIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(display, fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW(display, false);
            }

        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book2.");
            CHECK(false);
        }

        SendRemoteCommand(UP);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);

        ASSERT_VIEW_UNCHANGED(display, "MainMenu");

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display);
        WAIT_FOR_DRAW(display, false);

        ASSERT_VIEW_UNCHANGED(display, "ThirdScreen");

        // }

        // SECTION("Book 3") {

        SendRemoteCommand(DOWN);
        WAIT_FOR_DRAW(display);

        SendRemoteCommand(ENTER);
        WAIT_FOR_DRAW(display, false);

        screen = UIGetCurrentScreen();
        readableScreen = std::dynamic_pointer_cast<ReadableScreen>(screen);

        if (readableScreen != nullptr) {

            // ASSERT_VIEW_UNCHANGED(display, "OnePageTutorial");
            // SendRemoteCommand(ENTER);
            // WAIT_FOR_DRAW(display);

            int pageNbr = 1;
            int spineIdx = 0;

            for (;;) {
                StreamPlace streamPlace = readableScreen->getStreamPlace();

                if (streamPlace.getSpineIdx() != spineIdx) {
                    spineIdx = streamPlace.getSpineIdx();
                    pageNbr = 1;
                }

                std::stringstream fname;
                fname << "Book3/" << spineIdx + 1 << "-" << pageNbr++;

                ASSERT_VIEW_UNCHANGED(display, fname.str());

                if (readableScreen->checkAtEnd()) break;

                SendRemoteCommand(ENTER);
                WAIT_FOR_DRAW(display, false);
            }
        } else {
            UNSCOPED_INFO("Unable to get ReadableScreen instance for Book3.");
            CHECK(false);
        }
#endif
    }
}
