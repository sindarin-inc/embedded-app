#pragma once

#include <string>

// Default book questions JSON to use when the downloaded version is not available
// This will be used as a fallback until the user syncs and downloads a fresh copy
const char DEFAULT_BOOK_QUESTIONS_JSON[] = R"BOOK_QUESTIONS({
    "version": 1,
    "questions": [
        {
            "slug": "recap",
            "question": "What just happened?",
            "description": "Get a quick recap of recent events to help remember where you left off or understand what you just read",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "characters",
            "question": "Who are the characters?",
            "description": "Get a focused look at the characters active in this scene and what they're doing right now",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "dictionary",
            "question": "Define unfamiliar words",
            "description": "Get clear definitions and explanations for difficult or unusual vocabulary to better understand the text",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter"
            ]
        },
        {
            "slug": "quotes",
            "question": "Notable quotes",
            "description": "Find and understand key quotes that capture important moments, reveal character insights, or express meaningful themes",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "explain-concept",
            "question": "Explain this concept",
            "description": "Break down complex ideas, theories, or concepts into simpler terms to better understand what you're reading",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "analyze",
            "question": "Analyze this part",
            "description": "Explore the deeper meaning of this section, including its themes, symbolism, and significance to the larger story",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "history",
            "question": "What's the historical context?",
            "description": "Learn about the historical events, periods, or circumstances that provide important background for understanding the text",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "references",
            "question": "What are the literary references?",
            "description": "Discover references to other books, myths, or cultural works and understand how they enhance the meaning of the text",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "summary",
            "question": "Summarize this chapter",
            "description": "Get a comprehensive overview of the chapter's key events, character developments, and important plot points",
            "allowed_contexts": [
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "main-themes",
            "question": "What's the book about?",
            "description": "Get a high-level summary of the book's main themes and premise without spoilers for upcoming events",
            "allowed_contexts": [
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "relationships",
            "question": "Map character relationships",
            "description": "Understand how all the characters are connected to each other - their relationships, alliances, conflicts, and history together",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "science",
            "question": "Explain the science",
            "description": "Get clear explanations of scientific principles, theories, or concepts mentioned in the text",
            "allowed_contexts": [
                "current_page",
                "recent_pages",
                "current_chapter",
                "book_so_far",
                "full_book"
            ]
        },
        {
            "slug": "next",
            "question": "What happens next?",
            "description": "Get a preview of what's coming up in the story (WARNING: Contains spoilers!)",
           "allowed_contexts" : [ "next_pages", "rest_of_chapter", "rest_of_book" ]
}
    ],
    "contexts": [
        {
            "context_type": "current_page",
            "name": "Current page",
            "description": "Content from just the current page"
        },
        {
            "context_type": "recent_pages",
            "name": "Recent pages",
            "description": "Content from the last few pages you've read"
        },
        {
            "context_type": "current_chapter",
            "name": "Current chapter",
            "description": "Content from the current chapter"
        },
        {
            "context_type": "book_so_far",
            "name": "Book up until now",
            "description": "Everything up to your current position"
        },
        {
            "context_type": "next_pages",
            "name": "Next few pages (spoilers)",
            "description": "Preview of what happens in the next few pages (contains spoilers)"
        },
        {
            "context_type": "rest_of_chapter",
            "name": "Rest of this chapter (spoilers)",
            "description": "Everything left in this chapter (contains spoilers)"
        },
        {
            "context_type": "rest_of_book",
            "name": "Rest of the book (spoilers)",
            "description": "Everything from here to the end (contains spoilers)"
        },
        {
            "context_type": "full_book",
            "name": "Entire book (spoilers)",
            "description": "Entire book content (contains spoilers)"
        }
    ],
    "models": [
        {
            "id": "gpt-4o"
        },
        {
            "id": "gpt-4o-mini"
        },
        {
            "id": "gpt-4.5-preview"
        },
        {
            "id": "o3-mini"
        },
        {
            "id": "o1"
        }
    ]
})BOOK_QUESTIONS";
