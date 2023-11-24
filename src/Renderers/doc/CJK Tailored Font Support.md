## Notes on CJK Tailored font support

**CJK**: Chinese, Japanese and Korean

Here is the list of elements that were problematic and their correction done in this branch:

### Font generation

**The issue**: There is way too many characters to integrate in a single font with all cases of characters used in all EPubs. Often a single EPub may contain more than one written language (e.g. CJK with english or european passages).

**The solution**: An application named font-generator is responsible of generating a tailored font for a EPub. The font contains only the characters in use in the book using the GNU Unifont bitmap glyphs. The font is located (by hand for now) in the `ibmf/font.ibmf` path in the EPub.

- The CJK characters are located in the following code point groups: `U+2E80..U+9FFF`, `U+FE10..U+FE6F`, `U+FF00..U+FFEF`
- The characters used for CJK have their `advance` field set to `8` or `16` pixels wide.
- The non-CJK characters get their `advance` field set to `bitmap_width + 1` pixels wide.

### EPubFile class adjustments

The `EPubFile` class is modified to load the font in memory if present in the EPub when it is opened (using the new `void loadFont();` method). The following methods are now available in the class and used by the `Renderer`:

- `auto hasFont() -> bool;`
- `auto getFont() -> IBMFFont *;`

### Renderer adjustments

The renderer, when called to generate a page, checks for the presence of a tailored font. The display class is getting this font, if available, to generate the characters on screen.

---

**The issue**: `<br>` tags are used instead of `<p>` and `<br>` 

**The solution**: 

---
**The issue**: `space` character is not used. U+3000 is used to create some gap at the beginning of paragraphs. 

**The solution**: The `nextByte()` lambda in EPubTokenizer translates the U+3000 as a space for the parsing process. In the IBMFFont driver, U+3000 is now processed as a space too (maybe not required as it is translated in the `nextByte()` lambda)

---
**The Issue**: Sequences of CJK glyphs are often longer than the space available in a single line. Cutting the sequence arbitrarily may cause ponctuation characters to appear at wrong places.

**The solution**: 

---
**The issue**: CJK glyphs are not well processed by the optical kerning algorithm.

**The solution**: When a font is loaded from the EPub, the optical kerning algorithm is disabled.