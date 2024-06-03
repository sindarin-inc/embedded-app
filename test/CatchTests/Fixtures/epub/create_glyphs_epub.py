from ebooklib import epub


def create_test_epub_with_spaces():
    book = epub.EpubBook()

    # Set metadata
    book.set_identifier("id123456")
    book.set_title("Test EPUB with Spaces")
    book.set_language("en")

    # Create chapter with specific Unicode blocks
    unicode_blocks = {
        "Basic Latin": (0x0020, 0x007F),
        "Latin-1 Supplement": (0x0080, 0x00FF),
        "Latin Extended-A": (0x0100, 0x017F),
        "Latin Extended-B": (0x0180, 0x024F),
        "Spacing Modifier Letters": (0x02B0, 0x02FF),
        "Greek and Coptic": (0x0370, 0x03FF),
        "Cyrillic": (0x0400, 0x04FF),
        "General Punctuation": (0x2000, 0x206F),
        "Superscripts and Subscripts": (0x2070, 0x209F),
        "Currency Symbols": (0x20A0, 0x20CF),
        "User Defined": (0xE000, 0xE07C),
        "Alphabetic Presentation Forms": (0xFB00, 0xFB4F),
    }

    for block_name, (start, end) in unicode_blocks.items():
        content = f"<h1>{block_name}</h1>\n<p>"
        content += " ".join(chr(i) for i in range(start, end + 1))
        content += "</p>"
        chapter = epub.EpubHtml(title=block_name, file_name=f"{block_name}.xhtml", lang="en")
        chapter.content = content
        book.add_item(chapter)
        book.spine.append(chapter)

    # Add default NCX and Nav file
    book.add_item(epub.EpubNcx())
    book.add_item(epub.EpubNav())

    # Define Table Of Contents
    book.toc = [
        epub.Link(f"{block_name}.xhtml", block_name, block_name) for block_name in unicode_blocks.keys()
    ]

    # Write to the file
    epub.write_epub("glyphs.epub", book, {})


# Run the function to create the EPUB with spaces
create_test_epub_with_spaces()
