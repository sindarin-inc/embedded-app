## Menu Parameters Definition and Retrieval

The Renderer now supplies the capability of inserting menu parameters' lists in an EPub document. This feature permits the main sol application to interrupt the presentation of pages to the user, display a menu to him, and continue to present the pages where the sequence of pages was interrupted.

To do so, a menu parameters' list definition must be inserted at least once anywhere inside the `<body> .. </body>` of one of the spine's XHTML file that is part of the EPub document. The definition takes the following form:

```xml
<div id="sol-menu"><ul><li id="Key 1">Value 1</li> ... </ul></div>
```

The `Renderer::renderPage()` method takes a new fourth parameter named `bool retrieveMenuParameters`. When true, the Renderer will take action to retrieve the parameters in a `MenuParameters` type variable (that is defined as `SpiramOrderedMap<std::string, std::string>`), if the form is detected in the current page being parsed to be shown. 

If a valid MenuParameters has been retrieved, the renderPage() method will return the same page location (e.g. the first parameter of the `Renderer::renderPage()` call) that was requested. The page will not be shown on screen. The application can then retrieve the menu parameters using the `auto Renderer::getMenuParameters() -> const MenuParameters &` method. The ordered map will not be empty if a valid parameters list was retrieved.

Once the Sol application completes the processing of the menu, it can then request the interrupted page rendering to be done again. It is important to pass a `retrieveMenuParameters` parameter set to false. If not, the renderer will loop continuously on the menu parameters' list retrieval as described above.

In the `guy/menu-parameters-retrieval` branch, the `ReadableScreen` class has been modified to demonstrate the capability. When a menu parameters' list is retrieved, it is shown in the log, the interrupted page is then requested again as described above (here is the modification done at line 281):

```c++
    // Regular Reading
    bool retrieveMenuParameters = true;
    do {
        std::tie(newPlace_, darkPicturePercent) =
            renderer_.renderPage(readable_->place, 0, false, retrieveMenuParameters);
        if (retrieveMenuParameters) {
            const MenuParameters &menuParameters = renderer_.getMenuParameters();
            if (!menuParameters.empty()) {
                log_d("Menu Parameters:");
                for (auto &param : menuParameters) {
                    log_d("    %s: %s", param.first.c_str(), param.second.c_str());
                }
                retrieveMenuParameters = false;
                continue;
            }
        }

        break;
    } while (true);
```

If more than one menu parameters' list is required in a same document, both lists must be far enough to be on different displayed pages in the document. If not, the second menu parameters' list will not be retrievable.

[the End]
