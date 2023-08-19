## Changes to the Renderer Interface for TXT + EPub Support

To help in the definition of the interface to the Renderer, here are the changes to that interface (all of this can be modified at your desire). all the definitions will eventually be Tidy compliant...:

### The RendererStream sub-classes

Each of the `RendererStream` sub-classes constructors have a new optional parameter:  `StreamType streamType`. The `StreamType` is defined as follow: 

``` C++
enum class StreamType : uint8_t { TXT, EPUB, OTHER };
```

Here are all the sub-classes constructor definitions, showing the default optional parameter values:

``` C++
ContainerStream(StreamType streamType = StreamType::OTHER);
ContainerStream(T contained, StreamType streamType = StreamType::OTHER);

BufferStream(const uint8_t *data, int size, StreamType streamType = StreamType::OTHER);

StdStringStream(std::string &string, StreamType streamType = StreamType::TXT);
StdStringStream(SpiramString &string, StreamType streamType = StreamType::TXT);
// protected:
StdStringStream(std::shared_ptr<std::istream> stream, StreamType streamType = StreamType::TXT);
```

The following methods are available to help identify the `StreamType` of an instance of those classes:

``` C++
auto isA(StreamType streamType) const -> bool { return streamType == streamType_; }
auto getStreamType() const -> StreamType { return streamType_; }
```

The following class method is defined in the `RendererStream` class to select the proper `StreamType` based on the file path extension:

``` C++
static auto parseStreamType(const char *path) -> StreamType {
        if (HasSuffix(path, ".txt") || HasSuffix(path, ".txt.gz")) {
            return StreamType::TXT;
        }
        if (HasSuffix(path, ".epub")) {
            return StreamType::EPUB;
        }
        return StreamType::OTHER;
}
```

Here is an example of use (from the `FileSystem.hpp` file):

``` C++
auto FileSystem::newRendererStream(const char *path) -> std::shared_ptr<RendererStream> {
    log_d("Attempting to open: %s", path);
    auto file = open(path);
    if (!file) {
        log_e("Failed to open file: %s", path);
        return nullptr;
    }
    return std::make_shared<ContainerStream<SFile>>(
                file,
                RendererStream::parseStreamType(path)); // <---
}
```

### The Place struct

The `Models/Place.hpp` is defined as follow. I've added some useful methods to it (to check for equality and to get a std:string for %s formatting in `log_x` method calls (not debugged yet):

``` C++
#pragma once

#include <cinttypes>
#include <string>

#pragma pack(push, 1)
struct Place {
    uint32_t offset;
    Idx fileIdx;

    auto operator==(const Place &rhs) const -> bool {
        return (offset == rhs.offset) && (fileIdx == rhs.fileIdx);
    }

    auto str() const -> std::string {
        std::string s;
        s = "[offset:";
        s += std::to_string(offset);
        s.append(" ,fileIdx:");
        s += std::to_string(fileIdx);
        s.append("]");
        return s;
    }
};
#pragma pack(pop)
```

This is used everywhere in the Renderer interface to get or return a document location. From Sam's conversation, this Place definition is going to be modified...

### The Renderer class interface

Here is the Renderer interface (showing a new method: `renderFirstPage()`):

``` C++
Renderer(DisplaySystem &display);

auto setStream(std::shared_ptr<RendererStream> stream) -> void;
auto setCurrentPlace(Place newPlace) -> void;
auto getPreviousPlace() -> Place;
inline auto getCurrentPlace() const -> Place;
inline auto size() const -> uint32_t;

auto renderPage(Place fromPlace, int16_t numSkipLines = 0) -> Place;

inline auto renderFirstPage(int16_t numSkipLines = 0) -> Place;
auto getTotalLengthOfLines() const -> uint16_t;

inline auto getLengthOfLine(Idx lineIdx) const -> uint16_t;
inline auto getNumRenderedLines() const -> uint16_t;
inline auto getCurrentPageWordCount() const -> uint32_t;
```