#include <argparse.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#pragma clang diagnostic pop

#include <EPubFile/EPubFile.hpp>
#include <Renderer/Renderer.hpp>
#include <Unzipper/UnzipperFileStream.hpp>

#include "DisplayUtil.hpp"
#include "Storage/SFileUnzipperStream.hpp"
#include "UI/Fonts.hpp"

int main(int argc, char **argv) {
    argparse::ArgumentParser program("epub-render");

    program.add_argument("epubFilename")
        .help("Path to the .epub file")
        .action([](const std::string &value) { return std::filesystem::path(value); });

    program.add_argument("epubPosition")
        .help("Internal position in the EPUB file in the format 'file_name:byte_position'");

    program.add_argument("outFile")
        .help("Path to save the image (.png file)")
        .action([](const std::string &value) { return std::filesystem::path(value); });

    auto display = createDisplay();
    auto bb = (*display).defaultTextRect();
    auto defaultBoundingBoxString =
        StringFormat("{\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}", bb.x, bb.y, bb.width, bb.height);
    program.add_argument("-b", "--boundingBox")
        .help("Bounding box for the rendering (as json)")
        .default_value(defaultBoundingBoxString);

    program.add_argument("-f", "--font")
        .help("Font to use for rendering")
        .default_value("solSansSmall");

    program.add_argument("-l", "--lineSpacing")
        .help("Line spacing for the rendering")
        .default_value(0)
        .action([](const std::string &value) { return std::stoi(value); });

    program.add_argument("-r", "--readableType")
        .help("Type of readable to render (book or article)")
        .default_value("book");

    try {
        program.parse_args(argc, argv);
    } catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    auto epubFilename = program.get<std::filesystem::path>("epubFilename");
    auto epubPosition = program.get<std::string>("epubPosition");

    std::cout << "EPUB file: " << epubFilename << std::endl;

    auto colonPos = epubPosition.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "Invalid position format. Must be in the format 'file_name:byte_position'"
                  << std::endl;
        return 1;
    }
    auto placeBytePosition = std::stoul(epubPosition.substr(colonPos + 1));
    auto placeFilename = epubPosition.substr(0, colonPos);
    auto place = std::make_shared<EPubPlace>(placeBytePosition, placeFilename);
    std::cout << "Place: " << place->getHRef() << ":" << place->getOffset() << std::endl;

    auto boundingBox = program.get<std::string>("--boundingBox");
    DynamicJsonDocument boundingBoxJson(1024);
    auto error = ArduinoJson::deserializeJson(boundingBoxJson, boundingBox);
    if (error) {
        std::cerr << "Invalid bounding box JSON specified. Must be in the format "
                     "'{\"x\":0,\"y\":0,\"w\":0,\"h\":0}'"
                  << std::endl;
        return 1;
    }
    auto x = boundingBoxJson["x"].as<uint16_t>();
    auto y = boundingBoxJson["y"].as<uint16_t>();
    auto w = boundingBoxJson["w"].as<uint16_t>();
    auto h = boundingBoxJson["h"].as<uint16_t>();
    DisplayRect boundingBoxRect = {x, y, w, h};
    std::cout << "Bounding box: " << x << "," << y << "," << w << "," << h << std::endl;

    Font *font = &fontFace0;
    auto fontArg = program.get<std::string>("--font");
    if (fontArg == "solSansSmall") {
        // Leaving this here even though it's the default in case we change the default.
        font = &fontFace0;
    } else if (fontArg == "solSansMedium") {
        font = &fontFace1;
    } else if (fontArg == "solSansLarge") {
        font = &fontFace2;
    } else {
        std::cerr << "Invalid font specified. Must be one of 'solSansSmall', "
                     "'solSansMedium', 'solSansLarge' "
                  << std::endl;
        return 1;
    }
    std::cout << "Font: " << font << std::endl;

    auto lineSpacing = program.get<int>("--lineSpacing");
    std::cout << "Line spacing: " << lineSpacing << std::endl;

    auto outputPath = program.get<std::filesystem::path>("outFile");
    std::cout << "Output file: " << outputPath << std::endl;

    auto readableType = program.get<std::string>("--readableType");
    std::cout << "Readable type: " << readableType << std::endl;

    auto file = SFile(std::make_shared<VFSFile>(epubFilename.c_str(), FILE_READ, false));
    auto readable = std::make_shared<Readable>();
    readable->readableType = readableType;

    PreferencesStore prefs;
    auto solDrm = std::make_shared<SolDrm>(prefs);
    auto unzipperStream = std::make_shared<SFileUnzipperStream>(file);
    auto epubFile = std::make_shared<EPubFile>(unzipperStream, solDrm);
    epubFile->setIsAnArticle(readable->readableType == "article");
    auto epubStream = std::make_shared<EPubFileRendererStream>(epubFile);

    Renderer renderer(*display, boundingBoxRect, FontHandle{font});
    renderer.setStream(epubStream);
    renderer.renderPage(place);
    renderer.setLineSpacing(lineSpacing);

    display->updateDisplay(DisplaySystem::UpdateType::FULL, true);

    if (!stbi_write_png(outputPath.c_str(), CONFIG_DISPLAY_WIDTH, CONFIG_DISPLAY_HEIGHT, 1,
                        display->buffer().get(), 0)) {
        std::cerr << "Unable to save rendered image to " << outputPath << std::endl;
        return 1;
    }

    return 0;
}
