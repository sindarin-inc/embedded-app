## Notes on CJK Tailored font support

**CJK**: Chinese, Japanese and Korean

Here is the list of elements that were problematic and the actions taken in the `CJK-demonstration` branch:

### Font generation

**The issue**: There is way too many characters to integrate in a single font with all cases of characters used in all EPubs. Often a single EPub may contain more than one written language (e.g. CJK with english or european passages).

**The solution**: An application named IBMF-font-generator is responsible of generating a tailored font for a book. The font contains only the characters in use in the book, extracted from the GNU Unifont bitmap glyphs. The font is located (by hand for now) in the book's `ibmf/font.ibmf` path. In the book's manifest, an entry is added for the font using id="ibmf_font", media-type="application/font", href="ibmf/font.ibmf"

- The CJK characters are located in the following code point groups: `U+2E80..U+9FFF`, `U+FE10..U+FE6F`, `U+FF00..U+FFEF`
- The characters used for CJK have their `advance` field set to `16` pixels wide (monospaced).
- The non-CJK characters get their `advance` field set to `bitmap_width + 1` pixels wide (proportional).
- A table identifies the characters to be located on the left and on the right border of the advance width, used to set the horizontal offset of the bitmap.
- Some characters generated or used by the Renderer, are added to the font. Namely: 0..9 digits, G, [, ], - and o 

### EPubFile class adjustments

The `EPubFile` class is modified to load the font in memory if present in the EPub when it is opened (using the new `void loadFont();` method). The following methods are now available in the class and used by the `Renderer`:

- `auto hasFont() -> bool;`
- `auto getFont() -> IBMFFont *;`

### Renderer adjustments

The renderer is now using char32_t to parse all text in pcdata (e.g. text inside tag delimiters). The Display and IBMFFont driver have been modified to directly receive char32_t vectors in addition to UTF-8 vectors to accomodate this change. 

This is a major rearangement of the renderer capability. As Asian languages are mostly using non-ASCII characters, adjustements to the renderer algorithm to control the Asian language rendering in a UTF-8 context was becoming way too complex to do. 

The renderer, when called to generate a page, checks for the presence of a tailored embedded font. The display class is getting this font, if available, to generate the characters on screen.

---

**The issue**: `<br>` tags are used instead of `<p>` and `<br>` 

**The solution**: When a first <br> tag is encountered, a new-line is generated. If a second <br> tags is encountered, a new-paragraph token is then added to the stream of tokens. 

---
**The issue**: `space` character is not used. U+3000 is used to create some gap at the beginning of paragraphs. 

**The solution**: Gaps at beginning of paragraphs are removed. the U+3000 if present elsewhere will be shown.

---
**The Issue**: Sequences of CJK glyphs are often longer than the space available in a single line. Cutting the sequence arbitrarily may cause punctuation characters to appear at wrong places. The notion of word in asian languages is often non-existant beyond the use of punctuation characters to separate the stream of glyphs into tokens.

**The solution**: As glyphs are retrieved to construct the sequence of tokens, the following aspects are taken into consideration to break the sequence into tokens:

- Punctuation characters that follow a glyph
- When a sequence of glyph is larger than the available remaining space in the current line
- For non-CJK character sequences: when a space is encountered

The algorithm do its best to not leave a punctuation character alone at the beginning of a line (unless forced by a new-line request). In the tokenizer, left and right punctuation tables are set to help the parser identifies where and when it is suitable to break a sequence into tokens.

---
**The issue**: CJK glyphs must be presented as monospaced, not proportional.

**The solution**: When a font is loaded from the EPub, the optical kerning algorithm is disabled for CJK characters. The font generator fixes the advance parameter value of all CJK related glyphs to 16 pixels. For non-cjk characters, the optical kerning will be applied when two or more non-cjk characters are found in a row.

---
**The issue**: Some punctuation code points are tailored for vertical writing. They must be transformed for horizontal writing. This is the case when a book has been typesetted for vertical, bottom up reading.

**The solution**: A table in the Tokenizer (named `cjkTransList`) identifies the characters that need to be transformed: They are part of the table: https://www.unicode.org/charts/PDF/UFE30.pdf . These are used in vertical typewriting of books and are to be replaced with those that are part of the followings: https://en.wikipedia.org/wiki/CJK_Symbols_and_Punctuation or from ASCII characters. For the later, the font-generator application loads by default the corresponding characters required for horizontal typesetting when vertical punctuation characters are found in the book. 

The renderer is responsible to do the swap (from vertical to horizontal typesetting). This requires that both the font-generator (the `transList` table in `main.cpp`) and the renderer (the `cjkTransList` table in `EPubTokenizer.cpp`) be in synch for the translation table content.

### Limitations

The renderer does not support vertical right to left typesetting.

As the following have been tailored for the Sol Font, they are not available in the GNU Unifont:

- Superscript/subscript characters are not available
- The unknown character glyph (rounded rectangle with a question mark in it) is not available

Numbered lists, if present, are shown with ASCII digits.

The EPub Tokenizer is able to cope with both EPubs with or without a tailored embedded font. 

Txt documents rendering still need to be updated and verified. 

The **Content** menu entry doesn't show CJK characters for now. This is also the case for books title in the books list, the dictionary or summary if they contain CJK characters.

The font generator doesn't integrate the font in the EPub document. This must be done by hand. The font must be declared in the manifest of the content.opf file with id = "ibmf_font".

A lot of testing is still required if ever that code is expected to be migrated to the main branch.

### CJK Book preparation

An original CJK ebook must be prepared to be able to be shown in the demonstrator. The steps required are as follow:

1. A taylored font must be prepared using the IBMF-font-generator as explaned above. The font must then be integrated in the `.epub` file. A manifest entry in the `content.opf` must point to the font through the `href=` attribute and must be using `id="ibmf_font"`, `media-type="application/font"` attributes. The application is located here: `https://github.com/turgu1/IBMF-font-generator`.

2. The title, author and table of content chapters' title must be translated in English or any European language, as supported in the normal embedded Sol Font. This include information located in the `toc.ncx` and `content.opf`.

3. The ebook can then be uploaded on the Sol Server for preparation and inclusion in the ebooks server's database.

[The End]