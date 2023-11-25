## Notes on CJK Tailored font support

**CJK**: Chinese, Japanese and Korean

Here is the list of elements that were problematic and the actions taken in this branch:

### Font generation

**The issue**: There is way too many characters to integrate in a single font with all cases of characters used in all EPubs. Often a single EPub may contain more than one written language (e.g. CJK with english or european passages).

**The solution**: An application named font-generator is responsible of generating a tailored font for a EPub. The font contains only the characters in use in the book using the GNU Unifont bitmap glyphs. The font is located (by hand for now) in the `ibmf/font.ibmf` path in the EPub.

- The CJK characters are located in the following code point groups: `U+2E80..U+9FFF`, `U+FE10..U+FE6F`, `U+FF00..U+FFEF`
- The characters used for CJK have their `advance` field set to `16` pixels wide (monospaced).
- The non-CJK characters get their `advance` field set to `bitmap_width + 1` pixels wide (proportional).

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

**The solution**: As CJK characters are expected to be all monospaced (same width of 16 pixels) it is preferable to keep it that way for the moment.

---
**The Issue**: Sequences of CJK glyphs are often longer than the space available in a single line. Cutting the sequence arbitrarily may cause ponctuation characters to appear at wrong places.

**The solution**: 

---
**The issue**: CJK glyphs must be presented as monospaced, not proportional.

**The solution**: When a font is loaded from the EPub, the optical kerning algorithm is disabled. The font generator fixes the advance parameter value of all CJK related glyphs to 16 pixels.

---
**The issue**: Some punctuation code points are tailored for vertical writing. They must be transformed for hotizontal writing. This is the case when a book as been typesetted for vertical, bottom up reading.

**The solution**: A table in the Tokenizer identifies the characters that needs to be transformed: The following are part of that table: https://www.unicode.org/charts/PDF/UFE30.pdf . These are used in vertical typewriting of books and are to be replaced with those that are part of the followings: https://en.wikipedia.org/wiki/CJK_Symbols_and_Punctuation . For the later, the font-generator application loads by default the corresponding characters required for horizontal typesetting when vertical punctuation characters are found in the book.