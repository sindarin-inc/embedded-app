# EPub Renderer

CLI tool to render a page from an `.epub` document.

## Dependencies

* OpenSSL (`brew install openssl`)
* CMake (`brew install cmake`)

## Compile with CMake

```sh
mkdir build
cd build
cmake ..
make -j
```

## Run

```sh
./epub-render mybook.epub "OEBPS/Step_9780553898200_epub_p01_r1.htm:189537" out.png
```

## Usage

```sh
./epub-render --help
```

```
Usage: epub-render [--help] [--version] [--boundingBox VAR] [--font VAR] [--lineSpacing VAR] [--readableType VAR] epubFilename epubPosition outFile

Positional arguments:
  epubFilename        Path to the .epub file
  epubPosition        Internal position in the EPUB file in the format 'file_name:byte_position'
  outFile             Path to save the image (.png file)

Optional arguments:
  -h, --help          shows help message and exits
  -v, --version       prints version information and exits
  -b, --boundingBox   Bounding box for the rendering (as json) [nargs=0..1] [default: "{"x":30,"y":20,"w":196,"h":216}"]
  -f, --font          Font to use for rendering [nargs=0..1] [default: "solSansSmall"]
  -l, --lineSpacing   Line spacing for the rendering [nargs=0..1] [default: 0]
  -r, --readableType  Type of readable to render (book or article) [nargs=0..1] [default: "book"]
  ```
  