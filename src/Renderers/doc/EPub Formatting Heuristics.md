## EPub Formatting Heuristics:

Here is the list of all supported features that are part of EPub's XHTML files, and how the Renderer is displaying those features on screen.

**`<p><div><h1>..<h6>`** : Those constitute normal paragraphs. Words are tokenized and presented as a single paragraph, terminated with a small vertical gap to separate paragraphs. Empty paragraphs are bypassed. An empty paragraph contains only spaces or **`<span>`** tags with or without spaces. They are often used to generate easy vertical space in title pages instead of using CSS. When part of a list or blockquote, paragraphs are indented at the appropriate level, supporting nested lists and blockquotes.

Partially working: For header tags (**`<h1>..<h6>`**), the first line of the following paragraph will be kept on the same page: if there is not enough space at the end of the current page, the header will appears on next page.

**`<a>`** : Hyperlink. The text that is part of this tag is put in square brackets unless it is located inside a superscript, or when there is no text, or if the text starts with **`(`**, **`[`**, **`<`**, or **`{`**.

**`<picture>`** : Pictures are normally loaded in memory by the Pager class when it is ready to generate the content to be displayed. The server generated EPub documents have the width and height of pictures inserted as attributes to the <source> tag, this permits to optimize the moment the picture will be loaded when it is required (the width and height are sufficient to compute the page required space) as these pieces of information are part of the generated token. The pictures are loaded alone on a page when they are large enough to make it impossible to have paragraphs above or below them. Pictures are displayed using their native width and height. Only dithered pictures are selected for display. TBC

**`<ol><li>`** : Ordered lists use a default number sequence starting at 1 for each item in the list. For **`<ol>`** tag, a "start=" attribute would set the sequence number to start the numbering. For **`<li>`** tags, a "value=" attribute would set the number of that item, and subsequent list entries will increment from this number. When an item is taking more than one line, the following lines are indented to be aligned with the text of the first line located after the sequence number.

**`<ul><li>`** : Unordered lists are using bullets to mark the beginning of an item. The characters used as bullets are the **hyphen** for odd nested levels and the **lowercase o** for even nested levels (TBD: use nicer characters). When an item is taking more than one line, the following lines are indented to be aligned with the text of the first line located after the bullet.

Nested lists are allowed up to 5 levels. Both Ordered and Unordered lists can be used in nested lists. Each level of a nested list is indented from the preceding level. Items can be composed of paragraphs and blockquotes and are indented as well. 

**`<table>`** : Table formatting is partial: a table will start with "{TABLE}" shown at the beginning, and "{END TABLE}" shown at the end. Every row is separated with an horizontal rule. Rows are usually having one or more "cells" into them that are normally being shown in different columns. The renderer keeps the cells in separate lines: The screen width, the complexity of showing properly columns render the task of displaying them properly beyond the current capability: often the same cell must be aligned inside many columns or many rows, CSS is often used to control various aspect of cell formatting. Could be revisited in a subsequent version if better rendering solutions are identified.

**`<blockquote>`** : Left and Right indent are added to paragraphs that are part of a blockquote. The left and right indent are set to 12 and 8 pixels respectively for the first level. Blockquotes can be nested into each other. 8 pixels are added at additional level of indentation.

**`Space`** : Multiple space characters in a row are replaced with a single space. Paragraphs that contain only space characters are eliminated from the output (Often used to do easy placement in pages instead of using CSS). 

**`U+2000..U+200F`** : These code points represent various space characters. They are all replaced with a usual space glyph.

**`Hyphen`** : It is managed as a single-character word token. As lines are cut at word boundaries, this automatically offers the capability of splitting hyphenated compound words at the hyphens location. Endash and Emdash are expected to be entered as UTF8, or HTML character entities (**`&ndash;`**, **`&mdash;`**).

**`<br/>`** : Used to generate a break in paragraphs. Can be used in many different contexts, one of which is to do easy formatting instead of using CSS. Bad formatting occurs on the Sol as the available screen space is smaller than usually available with electronic readers. It is then replaced with a space character.

**`Tab`** : That character is managed as space characters (see the definition above). Could eventually be replaced with a displacement toward dedicated horizontal tabulator locations or other means. Would require many changes to the Pager class.

**`<sup><sub>`** : Superscript / Subscript processing is limited to the decimal digits 0 to 9, lower / upper case ASCII characters and the following: **`* - + / = [ ] ( ) . ,`**. Other characters are ignored. TBD: Superscript text needs to be glued (i.e. no newline before it) to the preceding word, or the following word (i.e. no newline after it) if located at the beginning of a paragraph as usually used for notes at the end of a chapter or section.

**`<hr/>`** : Generates a horizontal rule.

**`&...;`** : HTML character entities are translated to their UTF8 counterpart. Both numerical (e.g. **`&#1234;`** and textual (e.g. **`&nbsp;`**) descriptors are supported. All the supported descriptors as defined for HTML are described below. If a UTF8 character is present in the document, but not available in the font, it will be displayed as a question mark inside a rounded rectangle.

**`&nbsp;`** : Stand for "Not Breakable Space". Considered as part of a word, as for the other HTML character entities.

**`U+FFF0..U+FFFF`** : These code points are ignored. All other code points that are not available in the Sol font are shown as a question mark inside a rounded rectangle.

### Some notes:

- As we are not using CSS to format the content, there is many cases for which the pages could be showing things that are not expected. For example, items could be made hidden or displayed with specific characteristics depending on some information in CSS, like @media type that we don't interpret. Pictures could be shown in different flavors, sometimes being show two or three times in a row, the CSS-based selection not being interpreted.

- In Books, some formatting is made using HTML tags, doing a nice job for larger displays, but not that good for small screens like the Sol glasses.

- The Sol's font doesn't contain all possible characters. When a character is not available in the font, the driver replace it with a question mark inside a rounded rectangle.

- Some books are using specialized embedded fonts to display nice drawing effects in paragraphs or between them. If those drawings are encoded using the same byte code used for normal characters, those normal characters will then appear instead.

### Supported HTML Character Entities

|  Name   |  Description |
|:-------:|------------------------------------------
|  nbsp   | no-break space = non-breaking space, U+00A0 ISOnum
|  iexcl  | inverted exclamation mark, U+00A1 ISOnum
|  cent   | cent sign, U+00A2 ISOnum
|  pound  | pound sign, U+00A3 ISOnum
|  curren | currency sign, U+00A4 ISOnum
|  yen    | yen sign = yuan sign, U+00A5 ISOnum
|  brvbar | broken bar = broken vertical bar, U+00A6 ISOnum
|  sect   | section sign, U+00A7 ISOnum
|  uml    | diaeresis = spacing diaeresis, U+00A8 ISOdia
|  copy   | copyright sign, U+00A9 ISOnum
|  ordf   | feminine ordinal indicator, U+00AA ISOnum
|  laquo  | left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum
|  not    | not sign = angled dash, U+00AC ISOnum
|  shy    | soft hyphen = discretionary hyphen, U+00AD ISOnum
|  reg    | registered sign = registered trade mark sign, U+00AE ISOnum
|  macr   | macron = spacing macron = overline = APL overbar, U+00AF ISOdia
|  deg    | degree sign, U+00B0 ISOnum
|  plusmn | plus-minus sign = plus-or-minus sign, U+00B1 ISOnum
|  sup2   | superscript two = superscript digit two = squared, U+00B2 ISOnum
|  sup3   | superscript three = superscript digit three = cubed, U+00B3 ISOnum
|  acute  | acute accent = spacing acute, U+00B4 ISOdia
|  micro  | micro sign, U+00B5 ISOnum
|  para   | pilcrow sign = paragraph sign, U+00B6 ISOnum
|  middot | middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum
|  cedil  | cedilla = spacing cedilla, U+00B8 ISOdia
|  sup1   | superscript one = superscript digit one, U+00B9 ISOnum
|  ordm   | masculine ordinal indicator, U+00BA ISOnum
|  raquo  | right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum
|  frac14 | vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum
|  frac12 | vulgar fraction one half = fraction one half, U+00BD ISOnum
|  frac34 | vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum
|  iquest | inverted question mark = turned question mark, U+00BF ISOnum
|  Agrave | latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1
|  Aacute | latin capital letter A with acute, U+00C1 ISOlat1
|  Acirc  | latin capital letter A with circumflex, U+00C2 ISOlat1
|  Atilde | latin capital letter A with tilde, U+00C3 ISOlat1
|  Auml   | latin capital letter A with diaeresis, U+00C4 ISOlat1
|  Aring  | latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1
|  AElig  | latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1
|  Ccedil | latin capital letter C with cedilla, U+00C7 ISOlat1
|  Egrave | latin capital letter E with grave, U+00C8 ISOlat1
|  Eacute | latin capital letter E with acute, U+00C9 ISOlat1
|  Ecirc  | latin capital letter E with circumflex, U+00CA ISOlat1
|  Euml   | latin capital letter E with diaeresis, U+00CB ISOlat1
|  Igrave | latin capital letter I with grave, U+00CC ISOlat1
|  Iacute | latin capital letter I with acute, U+00CD ISOlat1
|  Icirc  | latin capital letter I with circumflex, U+00CE ISOlat1
|  Iuml   | latin capital letter I with diaeresis, U+00CF ISOlat1
|  ETH    | latin capital letter ETH, U+00D0 ISOlat1
|  Ntilde | latin capital letter N with tilde, U+00D1 ISOlat1
|  Ograve | latin capital letter O with grave, U+00D2 ISOlat1
|  Oacute | latin capital letter O with acute, U+00D3 ISOlat1
|  Ocirc  | latin capital letter O with circumflex, U+00D4 ISOlat1
|  Otilde | latin capital letter O with tilde, U+00D5 ISOlat1
|  Ouml   | latin capital letter O with diaeresis, U+00D6 ISOlat1
|  times  | multiplication sign, U+00D7 ISOnum
|  Oslash | latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1
|  Ugrave | latin capital letter U with grave, U+00D9 ISOlat1
|  Uacute | latin capital letter U with acute, U+00DA ISOlat1
|  Ucirc  | latin capital letter U with circumflex, U+00DB ISOlat1
|  Uuml   | latin capital letter U with diaeresis, U+00DC ISOlat1
|  Yacute | latin capital letter Y with acute, U+00DD ISOlat1
|  THORN  | latin capital letter THORN, U+00DE ISOlat1
|  szlig  | latin small letter sharp s = ess-zed, U+00DF ISOlat1
|  agrave | latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1
|  aacute | latin small letter a with acute, U+00E1 ISOlat1
|  acirc  | latin small letter a with circumflex, U+00E2 ISOlat1
|  atilde | latin small letter a with tilde, U+00E3 ISOlat1
|  auml   | latin small letter a with diaeresis, U+00E4 ISOlat1
|  aring  | latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1
|  aelig  | latin small letter ae = latin small ligature ae, U+00E6 ISOlat1
|  ccedil | latin small letter c with cedilla, U+00E7 ISOlat1
|  egrave | latin small letter e with grave, U+00E8 ISOlat1
|  eacute | latin small letter e with acute, U+00E9 ISOlat1
|  ecirc  | latin small letter e with circumflex, U+00EA ISOlat1
|  euml   | latin small letter e with diaeresis, U+00EB ISOlat1
|  igrave | latin small letter i with grave, U+00EC ISOlat1
|  iacute | latin small letter i with acute, U+00ED ISOlat1
|  icirc  | latin small letter i with circumflex, U+00EE ISOlat1
|  iuml   | latin small letter i with diaeresis, U+00EF ISOlat1
|  eth    | latin small letter eth, U+00F0 ISOlat1
|  ntilde | latin small letter n with tilde, U+00F1 ISOlat1
|  ograve | latin small letter o with grave, U+00F2 ISOlat1
|  oacute | latin small letter o with acute, U+00F3 ISOlat1
|  ocirc  | latin small letter o with circumflex, U+00F4 ISOlat1
|  otilde | latin small letter o with tilde, U+00F5 ISOlat1
|  ouml   | latin small letter o with diaeresis, U+00F6 ISOlat1
|  divide | division sign, U+00F7 ISOnum
|  oslash | latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1
|  ugrave | latin small letter u with grave, U+00F9 ISOlat1
|  uacute | latin small letter u with acute, U+00FA ISOlat1
|  ucirc  | latin small letter u with circumflex, U+00FB ISOlat1
|  uuml   | latin small letter u with diaeresis, U+00FC ISOlat1
|  yacute | latin small letter y with acute, U+00FD ISOlat1
|  thorn  | latin small letter thorn, U+00FE ISOlat1
|  yuml   | latin small letter y with diaeresis, U+00FF ISOlat1
|  quot   | quotation mark, U+0022 ISOnum
|  amp    | ampersand, U+0026 ISOnum
|  lt     | less-than sign, U+003C ISOnum
|  gt     | greater-than sign, U+003E ISOnum
|  apos   | apostrophe = APL quote, U+0027 ISOnum
|  OElig  | latin capital ligature OE, U+0152 ISOlat2
|  oelig  | latin small ligature oe, U+0153 ISOlat2
|  Scaron | latin capital letter S with caron, U+0160 ISOlat2
|  scaron | latin small letter s with caron, U+0161 ISOlat2
|  Yuml   | latin capital letter Y with diaeresis, U+0178 ISOlat2
|  circ   | modifier letter circumflex accent, U+02C6 ISOpub
|  tilde  | small tilde, U+02DC ISOdia
|  ensp   | en space, U+2002 ISOpub
|  emsp   | em space, U+2003 ISOpub
|  thinsp | thin space, U+2009 ISOpub
|  zwnj   | zero width non-joiner, U+200C NEW RFC 2070
|  zwj    | zero width joiner, U+200D NEW RFC 2070
|  lrm    | left-to-right mark, U+200E NEW RFC 2070
|  rlm    | right-to-left mark, U+200F NEW RFC 2070
|  ndash  | en dash, U+2013 ISOpub
|  mdash  | em dash, U+2014 ISOpub
|  lsquo  | left single quotation mark, U+2018 ISOnum
|  rsquo  | right single quotation mark, U+2019 ISOnum
|  sbquo  | single low-9 quotation mark, U+201A NEW
|  ldquo  | left double quotation mark,  U+201C ISOnum
|  rdquo  | right double quotation mark, U+201D ISOnum
|  bdquo  | double low-9 quotation mark, U+201E NEW
|  dagger | dagger, U+2020 ISOpub
|  Dagger | double dagger, U+2021 ISOpub
|  permil | per mille sign, U+2030 ISOtech
|  lsaquo | single left-pointing angle quotation mark, U+2039 ISO proposed
|  rsaquo | single right-pointing angle quotation mark, U+203A ISO proposed
|  euro   | euro sign, U+20AC NEW

[The End]
