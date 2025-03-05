#include "UI/BookQuestionsScreen.hpp"

#include <cinttypes>

#include "UI/Bitmaps/DefaultBookQuestions.hpp"

#if CONFIG_USE_BLE_SERVER_PROXY
#include "Communications/BLEManager.hpp"
#endif
#include "Communications/WiFi.hpp"
#include "Misc/StringUtil.hpp"
#include "UI/Fonts/Fonts.hpp"
#include "sindarin-debug.h"

void BookQuestionsScreen::moveSelection(int delta) {
    auto newPage = menuView_.moveSelection(delta);
    if (newPage) {
        setFirstLoad();
    }
    setNeedsRerender();
}

auto BookQuestionsScreen::layout(LayoutContext &context, SolSize size) -> SolSize {
    const auto inset = 2;
    auto fullFrame = SolRect::fromSize(size);

    // Set up the background frame first
    auto backgroundFrame = fullFrame.inset(0);
    context.setViewFrame(backgroundView_, backgroundFrame);

    // Position back label at top inside the background
    // Reduced horizontal insets to make the content area ~20 pixels wider
    // Left inset: 10 → 5, Right inset: 2 → 0
    auto contentFrame = backgroundFrame.inset(5, 5, 2, 3);
    auto backLabelRect = context.setViewFrame(backLabelView_, contentFrame,
                                              LayoutOptionsSizeToFit | LayoutOptionsAlignTop |
                                                  LayoutOptionsAlignLeft);

    // Handle QUESTIONS_LIST state - simplest case
    if (state_ == State::QUESTIONS_LIST) {
        context.setViewFrame(menuView_, contentFrame.inset(inset),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical);
        return size;
    }

    // For all other states, position question view below back label
    auto questionRect = context.setViewFrame(
        questionView_, contentFrame.inset(inset, inset, backLabelRect.bottom() + inset * 4, 0),
        LayoutOptionsSizeToFitVertical);

    // Handle CONTEXTS_LIST state
    if (state_ == State::CONTEXTS_LIST) {
        auto descRect = context.setViewFrame(
            descriptionView_, contentFrame.inset(inset, questionRect.bottom() + inset * 2),
            LayoutOptionsSizeToFitVertical);

        auto headingRect = context.setViewFrame(
            contextHeadingView_, contentFrame.inset(inset, descRect.bottom() + inset * 4),
            LayoutOptionsSizeToFitVertical);

        context.setViewFrame(menuView_, contentFrame.inset(inset, headingRect.bottom() + inset * 2),
                             LayoutOptionsSizeToFitVertical);
        return size;
    }

    // Handle MODEL_LIST state
    if (state_ == State::MODEL_LIST) {
        auto descRect = context.setViewFrame(
            descriptionView_, contentFrame.inset(inset, questionRect.bottom() + inset * 2),
            LayoutOptionsSizeToFitVertical);

        auto headingRect = context.setViewFrame(
            contextHeadingView_, contentFrame.inset(inset, descRect.bottom() + inset * 4),
            LayoutOptionsSizeToFitVertical);

        context.setViewFrame(menuView_, contentFrame.inset(inset, headingRect.bottom() + inset * 2),
                             LayoutOptionsSizeToFitVertical);
        return size;
    }

    // Handle LOADING state
    if (state_ == State::LOADING) {
        context.setViewFrame(questionView_, contentFrame,
                             LayoutOptionsSizeToFit | LayoutOptionsAlignCenter |
                                 LayoutOptionsAlignCenterVertical);
        context.setViewFrame(descriptionView_,
                             // Make the description text narrower for better wrapping
                             contentFrame.inset(contentFrame.size.width / 6, 0),
                             LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenter);
        return size;
    }

    // Handle ANSWER state
    if (state_ == State::ANSWER) {
        if (!questionResponse_.success) {
            // Center the error text vertically in the content area
            context.setViewFrame(descriptionView_,
                                 // Make the description text narrower for better wrapping
                                 contentFrame.inset(contentFrame.size.width / 6),
                                 LayoutOptionsSizeToFitVertical | LayoutOptionsAlignCenterVertical |
                                     LayoutOptionsAlignCenter);
        } else {
            // Position page indicator at the bottom of the screen with some padding
            context.setViewFrame(pageIndicatorView_,
                                 contentFrame.inset(0, inset * 2), // Double inset at the bottom
                                 LayoutOptionsSizeToFit | LayoutOptionsAlignBottom |
                                     LayoutOptionsAlignCenterHorizontal);
        }

        return size;
    }

    return size;
}

void BookQuestionsScreen::drawInRect(SolRect rect) {
    // Always draw background first
    backgroundView_.fillColor = display_.bg;
    backgroundView_.strokeColor = display_.fg;
    backgroundView_.draw();

    // Always draw back label
    backLabelView_.draw();

    switch (state_) {
    case State::QUESTIONS_LIST:
        menuView_.draw();
        break;

    case State::CONTEXTS_LIST:
    case State::MODEL_LIST:
        questionView_.draw();
        descriptionView_.draw();
        contextHeadingView_.draw();
        menuView_.draw();
        break;

    case State::LOADING:
        questionView_.draw();
        descriptionView_.draw();
        break;

    case State::ANSWER:
        if (!questionResponse_.success) {
            descriptionView_.draw();
        }
        // Note: For successful ANSWER state, the actual answer content
        // is rendered directly in the render() method using the renderer
        break;
    }
}

void BookQuestionsScreen::precomputeTotalPages() {
    // Save current renderer stream
    auto originalStream = renderer_.getStream();

    // Create a stream with the answer text
    auto stream = std::make_shared<StdStringStream>(questionResponse_.answer, DocType::TXT);
    renderer_.setStream(stream);

    // Set up renderer with same dimensions as we'll use for actual rendering
    auto rect =
        DisplayRect::fromSizeAndInsets(display_.width(), display_.height(), {30, 30, 30, 45});
    rect = rect.inset(10, 10, 15, 0);
    renderer_.setRect(rect);

    // Create a temporary place for counting pages
    auto countPlace = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
    size_t pageCount = 0;
    pageBreakOffsets_.clear();
    pageNumbers_.clear();

    // Keep rendering pages until we hit the end
    do {
        uint8_t darkPicturePercent = 0;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(countPlace);
        countPlace = std::dynamic_pointer_cast<TxtPlace>(newPlace);
        pageCount++;
        pageBreakOffsets_.push_back(countPlace->getOffset());
        pageNumbers_.push_back(pageCount);
    } while (!renderer_.atEndOfFile());

    // Store the result
    precomputedTotalPages_ = pageCount;

    // Restore original stream
    renderer_.setStream(originalStream);
}

void BookQuestionsScreen::render(DisplaySystem &display) {
    if (needsMenuItemsLoad_) {
        updateUIForState();
        needsMenuItemsLoad_ = false;
    }

    setNeedsLayout();
    setFrame(defaultFrameRect());

    if (state_ == State::ANSWER && questionResponse_.success) {
        log_d("RENDER: ANSWER state: current page=%zu", currentPage_);
        // Save the current renderer stream if needed
        // Note: We should already have saved it in executeQuestion, but check just in case
        if (rendererStream_ == nullptr) {
            log_d("RENDER: Saving renderer stream in ANSWER state");
            rendererStream_ = renderer_.getStream();
        }

        // Create a stream with the answer text
        auto stream = std::make_shared<StdStringStream>(questionResponse_.answer, DocType::TXT);
        renderer_.setStream(stream);

        // Draw the background first
        draw();

        // Double up on the vertical insets like ReadableDictionaryScreen/ReadableSummaryScreen
        auto rect =
            DisplayRect::fromSizeAndInsets(display_.width(), display_.height(), {30, 30, 30, 45});
        rect = rect.inset(10, 10, 15, 0);
        renderer_.setRect(rect);

        // Set the place based on the current page
        if (answerPlace_ == nullptr) {
            // First time rendering, create a new place at the beginning
            answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
        } else if (currentPage_ < pageBreakOffsets_.size()) {
            // For pages other than the first, set the offset based on the previous page's break
            size_t offset = pageBreakOffsets_[currentPage_ - 1];
            log_d("RENDER: Setting place to offset %zu for page %zu", offset, currentPage_);
            answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(offset));
        } else {
            log_d("RENDER: Already at end of answer, can't page forward");
        }

        // Render the page
        uint8_t darkPicturePercent = 0;
        std::shared_ptr<Place> newPlace;
        std::tie(newPlace, darkPicturePercent) = renderer_.renderPage(answerPlace_);
        answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(newPlace);

        // Get the current page number based on offset
        auto currentOffset = answerPlace_->getOffset();
        log_d("RENDER: Current offset=%" PRIu32 ", Current page=%zu/%zu", currentOffset,
              currentPage_, precomputedTotalPages_);

        // Update page indicator with precomputed total and current page
        pageIndicatorView_.setTotalPages(precomputedTotalPages_);
        pageIndicatorView_.setCurrentPage(currentPage_);
        // Draw page indicator in answer state
        pageIndicatorView_.draw();

        // Update display
        if (prefs_.fastTransitions.get().value_or(false)) {
            display.updateDisplay(DisplaySystem::UpdateType::PARTIAL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        } else {
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true,
                                  display.darkPictureVComAdj(darkPicturePercent));
        }
    } else {
        draw();

        if (firstLoad_) {
            log_w("RENDER: Updating display to full on first load");
            display.updateDisplay(DisplaySystem::UpdateType::FULL, false);
            firstLoad_ = false;
        } else if (state_ == State::ANSWER || state_ == State::LOADING) {

            log_w("RENDER: Updating display to full");
            display.updateDisplay(DisplaySystem::UpdateType::FULL, true);
        }

        // Only draw fast elements when in a state that shows the menu
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST ||
            state_ == State::MODEL_LIST) {
            log_w("RENDER: Drawing fast elements");
            menuView_.drawFastElements();
            display.updateDisplay(DisplaySystem::UpdateType::FAST_INDICATOR);
        } else {
            display.clearFrameBuffer();
        }
    }
}

void BookQuestionsScreen::processCommand(RemoteCommand command) {
    switch (command) {
    case RemoteCommand::UP:
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST ||
            state_ == State::MODEL_LIST) {
            moveSelection(-1);
        } else if (state_ == State::LOADING || state_ == State::ANSWER) {
            // Dismiss the dialog
            dismissDialog();
        }
        break;
    case RemoteCommand::DOWN:
        if (state_ == State::QUESTIONS_LIST || state_ == State::CONTEXTS_LIST ||
            state_ == State::MODEL_LIST) {
            moveSelection(1);
        }
        break;
    case RemoteCommand::ENTER:
    case RemoteCommand::RIGHT:
        if (state_ == State::QUESTIONS_LIST) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex < questions_.questions.size()) {
                selectedQuestionIndex_ = selectedIndex;
                selectedQuestion_ = questions_.questions[selectedIndex];
                setState(State::CONTEXTS_LIST);
            }
        } else if (state_ == State::CONTEXTS_LIST) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex >= questions_.contexts.size()) {
                log_d("Invalid context index: %zu", selectedIndex);
                return;
            }
            selectedContextIndex_ = selectedIndex;
            selectedContext_ = questions_.contexts[selectedIndex];

            // If debug menu is enabled and we have models, go to model selection
            if (prefs_.showDebugMenu.get().value_or(false) && !questions_.models.empty()) {
                setState(State::MODEL_LIST);
            } else {
                log_d("Not showing model list (debug: %d, models: %zu)",
                      prefs_.showDebugMenu.get().value_or(false), questions_.models.size());
                // Otherwise, execute the question directly
                executeQuestion(questions_.contexts[selectedIndex]);
            }
        } else if (state_ == State::MODEL_LIST) {
            auto selectedIndex = menuView_.getSelectedIndex();
            if (selectedIndex >= questions_.models.size()) {
                log_d("Invalid model index: %zu", selectedIndex);
                return;
            }
            selectedModelIndex_ = selectedIndex;
            selectedModelId_ = questions_.models[selectedIndex].id;
            executeQuestion(selectedContext_, std::string(selectedModelId_));
        } else if (state_ == State::ANSWER) {
            // Page forward in answer
            log_d("NAVIGATION: Paging forward in answer");
            if (questionResponse_.success && currentPage_ < precomputedTotalPages_ - 1) {
                log_d("NAVIGATION: Paging forward in answer, current page=%zu", currentPage_);
                currentPage_++;
                setNeedsRerender();
            } else if (!questionResponse_.success) {
                // For error states, just go back
                dismissDialog();
            } else {
                log_d("NAVIGATION: Already at end of answer, can't page forward");
            }
        }
        break;
    case RemoteCommand::BACK:
    case RemoteCommand::LEFT:
        switch (state_) {
        case State::QUESTIONS_LIST:
            dismissDialog();
            break;
        case State::ANSWER:
            if (!questionResponse_.success) {
                // For error states, just go back
                dismissDialog();
            } else {
                // Try to page backward in answer
                if (currentPage_ > 0) {
                    log_d("NAVIGATION: Paging backward in answer, current page=%zu", currentPage_);
                    currentPage_--;
                    setNeedsRerender();
                } else {
                    log_d("NAVIGATION: Already at beginning of answer, can't page backward");
                }
            }
            break;
        case State::CONTEXTS_LIST:
            setState(State::QUESTIONS_LIST);
            menuView_.setSelectedIndex(selectedQuestionIndex_);
            break;
        case State::MODEL_LIST:
            setState(State::CONTEXTS_LIST);
            menuView_.setSelectedIndex(selectedContextIndex_);
            break;
        case State::LOADING:
            // Special handling for LOADING state to ensure proper cleanup
            dismissDialog();
            break;
        }
        break;
    default:
        break;
    }
}

void BookQuestionsScreen::loadQuestions() {
    // Load and parse questions JSON
    SFile questionsFile = fileSystem_.open("/book_questions.json", FILE_READ);
    if (!questionsFile) {
        log_w("Book questions file not found, using default compiled version");
        // Use the default compiled-in book questions JSON
        questions_ = BookQuestions::loadFromJson(DEFAULT_BOOK_QUESTIONS_JSON);
        telemetry_.logMetricData(
            TelemetryField::METRIC_DIALOG,
            {{"dialog", std::string("book-questions")}, {"action", std::string("using-default")}});
        return;
    }

    std::string jsonStr;
    while (questionsFile.available()) {
        jsonStr += static_cast<char>(questionsFile.read());
    }
    questionsFile.close();

    questions_ = BookQuestions::loadFromJson(jsonStr);
    log_d("Loaded book questions from file: bytes: %zu, version: %d, number of questions: %zu, "
          "number of contexts: %zu, number of models: %zu",
          jsonStr.length(), questions_.version, questions_.questions.size(),
          questions_.contexts.size(), questions_.models.size());
}

void BookQuestionsScreen::executeQuestion(const BookQuestionContext &context) {
    // Call the overloaded method with an empty model string
    executeQuestion(context, "");
}

void BookQuestionsScreen::executeQuestion(const BookQuestionContext &context,
                                          const std::string &modelId) {
    log_d("Executing question, context=%s, model=%s", context.contextType.c_str(), modelId.c_str());

    // Clear any previous state
    display_.clearDisplay();

    // Reset state
    setState(State::LOADING);
    setLoadingMessage("Connecting to WiFi...");

    // Reset dismissed flag when starting a new question
    dismissed_ = false;
    cancelFlag_ = std::make_shared<CancelFlag>();

    // Save the current renderer stream as soon as we start executing a question
    // This ensures we have it available for proper cleanup in any state
    if (rendererStream_ == nullptr) {
        log_d("Saving renderer stream in executeQuestion");
        rendererStream_ = renderer_.getStream();
    }

    // Get current place from the stream directly
    if (!placeInReadable_) {
        log_e("Failed to get current place");
        telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                 {{"dialog", std::string("book-questions")},
                                  {"action", std::string("failed-get-place")}});
        back();
        return;
    }

    log_d("Executing question: slug=%s, context=%s, model=%s", selectedQuestion_.slug.c_str(),
          context.contextType.c_str(), modelId.c_str());
    log_d("Book UUID: %s", readable_->uuid.c_str());
    log_d("Chapter: %s, Offset: %" PRIu32, placeInReadable_->getHRef().c_str(),
          placeInReadable_->getOffset());

    // Log telemetry for question execution
    telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                             {{"dialog", std::string("book-questions")},
                              {"action", std::string("execute-question")},
                              {"question_slug", selectedQuestion_.slug},
                              {"context_type", context.contextType},
                              {"model", modelId},
                              {"book_uuid", readable_->shortenedUuid},
                              {"chapter", placeInReadable_->getHRef()},
                              {"chapter_offset", placeInReadable_->getOffset()}});

    BookQuestionRequest request = {};
    request.questionSlug = selectedQuestion_.slug;
    request.contextType = context.contextType;
    request.bookUuid = readable_->uuid;
    request.chapter = placeInReadable_->getHRef();
    request.chapterOffset = placeInReadable_->getOffset();
    request.model = modelId;

    // Dispatch the API call to a background task
    DispatchGetCore0BackgroundQueue()->dispatch(
        [this, request]() { sendQuestionRequest(request, false); });
}

auto BookQuestionsScreen::connectToNetwork(bool useBleOnly, std::string &errorMessage) -> bool {
    bool wifiConnected = false;
    bool bleConnected = false;

    SindarinAPI::getInstance()->setAccessToken(prefs_.cloudToken.get().value());

    // Try WiFi connection if not in BLE-only mode
    if (!useBleOnly && !prefs_.wifiDisabled.get().value_or(false)) {
        log_i("Attempting WiFi connection...");
        auto connected = WiFi::getInstance().connectWiFiFromSettings();
        if (connected != ESP_OK && connected != true) {
            log_w("WiFi connection failed, logging metric");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "book-questions-wifi-failed");
            wifiConnected = false;
        } else {
            log_i("WiFi connection successful");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "book-questions-wifi-connected");
            wifiConnected = true;
        }
    } else if (useBleOnly) {
        log_i("BLE-only mode, skipping WiFi connection");
    } else {
        log_i("WiFi is disabled in preferences, skipping WiFi connection");
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    // Try BLE if WiFi failed or in BLE-only mode
    if ((!wifiConnected || useBleOnly) && !prefs_.bleDisabled.get().value_or(false)) {
        log_i("Attempting BLE connection...");

        // Disconnect WiFi if we're using BLE
        if (WiFi::getInstance().connected()) {
            log_i("Disconnecting WiFi since BLE is connecting");
            WiFi::getInstance().disconnectWiFi(3000);
        }

        DispatchGetUiQueue()->dispatch([this]() {
            if (!dismissed_) {
                setLoadingMessage("Connecting via Bluetooth...");
            }
        });

        if (!BLEManager::getInstance()->connectProxy(cancelFlag_)) {
            log_w("BLE connection failed");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "book-questions-ble-failed");
        } else {
            log_i("BLE successfully connected");
            telemetry_.logMetric(TelemetryField::METRIC_WIFI, "book-questions-ble-connected");
            bleConnected = true;
        }
    }
#endif

    if (!wifiConnected && !bleConnected) {
        log_w("Failed to establish any network connection");
        if (prefs_.wifiDisabled.get().value_or(false)) {
            errorMessage = "WiFi is disabled in settings. Please enable WiFi to use this feature.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("book-questions")},
                                      {"action", std::string("book-questions-failed")},
                                      {"reason", std::string("wifi-disabled")}});
        } else if (prefs_.bleDisabled.get().value_or(false)) {
            errorMessage = "Both WiFi and Bluetooth are disabled. Please enable one of them to "
                           "use this feature.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("book-questions")},
                                      {"action", std::string("book-questions-failed")},
                                      {"reason", std::string("ble-disabled")}});
        } else {
            errorMessage =
                "Could not connect to WiFi. Please check your WiFi settings and try again.";
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("book-questions")},
                                      {"action", std::string("book-questions-failed")},
                                      {"reason", std::string("wifi-connection-failed")}});
        }
        return false;
    }

    return true;
}

void BookQuestionsScreen::sendQuestionRequest(const BookQuestionRequest &request, bool useBleOnly) {
    // Check if screen has been dismissed before proceeding
    if (dismissed_) {
        log_i("Screen was dismissed, skipping question request");
        return;
    }

    std::string errorMessage;
    bool connected = connectToNetwork(useBleOnly, errorMessage);

    if (!connected) {
        DispatchGetUiQueue()->dispatch([this, errorMessage]() {
            // Check if screen has been dismissed before updating UI
            if (dismissed_) {
                log_i("Screen was dismissed, ignoring error message");
                return;
            }

            questionResponse_.success = false;
            questionResponse_.error = errorMessage;
            log_e("Question request failed: error='%s'", errorMessage.c_str());
            setState(State::ANSWER);
            setNeedsRerender();
        });
        return;
    }

    // Update UI to show we're waiting for the answer
    log_i("Network connected, sending question request to API");
    DispatchGetUiQueue()->dispatch([this]() {
        // Check if screen has been dismissed before updating UI
        if (dismissed_) {
            log_i("Screen was dismissed, ignoring loading message update");
            return;
        }

        setLoadingMessage("Waiting for answer...");
        render(display_);
    });

    // Execute API call in background
    log_i("Executing API request: question=%s, context=%s, model=%s, book=%s, chapter=%s, "
          "offset=%zu",
          request.questionSlug.c_str(), request.contextType.c_str(), request.model.c_str(),
          request.bookUuid.c_str(), request.chapter.c_str(), request.chapterOffset);

    // Check if already dismissed before making API call
    if (dismissed_) {
        log_i("Screen was dismissed, skipping API request");
        return;
    }

    auto resp = std::make_shared<BookQuestionResponse>();

    // Determine if we're using BLE based on connection state
    bool usingBle = false;
#if CONFIG_USE_BLE_SERVER_PROXY
    auto bleManager = BLEManager::getInstance(false);
    usingBle = (bleManager != nullptr && bleManager->connectedProxy);
#endif

    int result =
        SindarinAPI::getInstance()->askQuestion(request, resp.get(), usingBle, cancelFlag_);

    // Dispatch UI updates back to main thread
    log_i("API request completed with result=%d, success=%d", result, resp->success);

    DispatchGetUiQueue()->dispatch([this, result, resp, request, useBleOnly]() {
        // Check if screen has been dismissed before updating UI
        if (dismissed_) {
            log_i("Screen was dismissed, ignoring API response");
            return;
        }

        log_d("Processing API response: result=%d, success=%d", result, resp->success);

        if (result == 200 && resp->success) {
            log_i("Question request successful, answer length: %zu", resp->answer.length());
            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("book-questions")},
                                      {"action", std::string("question-success")},
                                      {"answer_length", static_cast<int>(resp->answer.length())}});

            questionResponse_ = *resp;
            setState(State::ANSWER);
        } else {
            log_e("Question request failed: result=%d, success=%d, error='%s'", result,
                  resp->success, resp->error.c_str());

            // If WiFi failed and we haven't tried BLE-only yet, retry with BLE
            if (!useBleOnly && !resp->success) {
                log_i("Retrying with BLE-only connection");
                telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                         {{"dialog", std::string("book-questions")},
                                          {"action", std::string("retry-with-ble")}});

                setLoadingMessage("Retrying with Bluetooth...");

                // Dispatch to background queue to retry with BLE-only
                DispatchGetCore0BackgroundQueue()->dispatch(
                    [this, request]() { sendQuestionRequest(request, true); });
                return;
            }

            telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                     {{"dialog", std::string("book-questions")},
                                      {"action", std::string("question-failed")},
                                      {"error", resp->error}});

            questionResponse_.success = false;
            questionResponse_.error =
                resp->error.empty() ? "Failed to get answer from server" : resp->error;
            setState(State::ANSWER);
        }
    });
}

void BookQuestionsScreen::setState(State newState) {
    state_ = newState;

    // Precompute total pages when entering ANSWER state
    if (state_ == State::ANSWER && questionResponse_.success) {
        precomputeTotalPages();

        // Reset page tracking
        currentPage_ = 0;
        answerPlace_ = nullptr;

        // Explicitly set the page indicator values after precomputing
        pageIndicatorView_.setTotalPages(precomputedTotalPages_);
        pageIndicatorView_.setCurrentPage(0); // Start at the first page

        // Log telemetry for answer display
        telemetry_.logMetricData(TelemetryField::METRIC_DIALOG,
                                 {{"dialog", std::string("book-questions")},
                                  {"action", std::string("answer-displayed")},
                                  {"total_pages", static_cast<int>(precomputedTotalPages_)}});
    }

    updateBackLabel();
    updateUIForState();
    setFirstLoad();
}

void BookQuestionsScreen::updateUIForState() {
    menuView_.resetMenuItems();

    switch (state_) {
    case State::QUESTIONS_LIST:
        if (questions_.questions.empty()) {
            loadQuestions();
            // With our default questions implementation, this should never happen
            // but keeping as a safeguard
            if (questions_.questions.empty()) {
                log_e("Failed to load default book questions");
                // Set state to LOADING to use the centered message layout
                state_ = State::LOADING;
                // Use all caps for the title to make it more prominent
                descriptionView_.setText("Go to Settings > Content > Sync to load questions.");
                // Update back label to show "Back" instead of "Dismiss"
                updateBackLabel();

                telemetry_.logMetricData(
                    TelemetryField::METRIC_DIALOG,
                    {{"dialog", std::string("book-questions")},
                     {"action", std::string("failed-load-default-questions")}});
                return;
            }
        }
        for (const auto &question : questions_.questions) {
            menuView_.addMenuItem(
                std::make_shared<MenuItemSingleLine>(std::string(question.question), []() {}));
        }
        break;

    case State::CONTEXTS_LIST:
        questionView_.setText(std::string(selectedQuestion_.question));
        descriptionView_.setText(std::string(selectedQuestion_.description));
        contextHeadingView_.setText("Answer using these pages as context:");
        for (const auto &context : questions_.contexts) {
            if (std::find(selectedQuestion_.allowedContexts.begin(),
                          selectedQuestion_.allowedContexts.end(),
                          context.contextType) != selectedQuestion_.allowedContexts.end()) {
                menuView_.addMenuItem(
                    std::make_shared<MenuItemSingleLine>(std::string(context.name), []() {}));
            }
        }
        break;

    case State::MODEL_LIST:
        questionView_.setText(std::string(selectedQuestion_.question));
        descriptionView_.setText(std::string(selectedContext_.description));
        contextHeadingView_.setText("Select a model to use:");
        for (const auto &model : questions_.models) {
            menuView_.addMenuItem(
                std::make_shared<MenuItemSingleLine>(std::string(model.id), []() {}));
        }
        break;

    case State::LOADING:
        break;

    case State::ANSWER:
        if (questionResponse_.success) {
            questionView_.setText(std::string(selectedQuestion_.question));
            // The answer text will be rendered by the renderer in render(), don't set it here
            descriptionView_.setText("");
        } else {
            questionView_.setText("");
            descriptionView_.setText(std::string(questionResponse_.error.c_str()));
        }
        setNeedsRerender();
        render(display_);
        break;
    }
}

void BookQuestionsScreen::updateBackLabel() {
    if (state_ == State::LOADING || state_ == State::ANSWER) {
        // For error screens, use "Back" instead of "Dismiss"
        if (state_ == State::LOADING && questions_.questions.empty()) {
            backLabelView_.setText("Back");
            backLabelView_.setArrowDirection(ArrowLeft);
        } else {
            backLabelView_.setText("Dismiss");
            backLabelView_.setArrowDirection(ArrowUp);
        }
    } else {
        backLabelView_.setText("Back");
        backLabelView_.setArrowDirection(ArrowLeft);
    }
}

void BookQuestionsScreen::restoreRendererStream() {
    log_d("Restoring renderer stream, current state=%d", static_cast<int>(state_));

    // First, make sure we're not in a state that would cause issues
    // Note: This state reset is now handled in dismissDialog() for problematic states
    // We keep it here for safety in case this method is called directly

    // Clear the display before restoring the renderer stream
    // This ensures no remnants of the BookQuestionsScreen remain visible
    display_.clearDisplay();

    if (rendererStream_ == nullptr) {
        log_w("Renderer stream is null, cannot restore. This may happen if the stream was never "
              "saved.");
        // Even without a stream to restore, we don't need to update the display here
        // The ReadableScreen will handle the display update
        return;
    }

    log_d("Setting renderer rect and restoring stream");
    // Set the renderer rect to the full text area
    renderer_.setRect(GetTextRectFromPrefs(display_, prefs_));

    // Store the stream locally before clearing the member variable
    auto streamToRestore = rendererStream_;
    rendererStream_ = nullptr;

    // Now restore the stream
    renderer_.setStream(streamToRestore);
}

void BookQuestionsScreen::disconnectNetworkConnections() {
    // Disconnect WiFi if connected
    if (WiFi::getInstance().connected()) {
        log_i("Disconnecting WiFi on dismiss");
        WiFi::getInstance().disconnectWiFi(3000);
    }

#if CONFIG_USE_BLE_SERVER_PROXY
    // Disconnect BLE if connected
    auto bleManager = BLEManager::getInstance(false);
    if (bleManager != nullptr && bleManager->connectedProxy) {
        log_i("Disconnecting BLE on dismiss");
        bleManager->disconnectProxy();
    }
#endif

    // Cancel any ongoing requests
    if (cancelFlag_) {
        log_i("Cancelling ongoing requests");
        CancelFlag::cancel(cancelFlag_);
    }
}

void BookQuestionsScreen::dismissDialog() {
    log_d("Dismissing dialog, state=%d", static_cast<int>(state_));

    // Always disconnect network connections first
    disconnectNetworkConnections();

    // Set dismissed flag to prevent background tasks from updating UI after dismissal
    dismissed_ = true;

    // Log telemetry
    telemetry_.logMetricData(
        TelemetryField::METRIC_DIALOG,
        {{"dialog", std::string("book-questions")}, {"action", std::string("dismissed")}});

    // Restore the renderer stream
    restoreRendererStream();

    // Set first load flag to ensure proper rendering when this screen is shown again
    setFirstLoad();
    setNeedsRerender();

    // Go back to the previous screen
    back();
}

void BookQuestionsScreen::setLoadingMessage(const std::string &message) {
    // Don't update UI if screen has been dismissed
    if (dismissed_ || state_ != State::LOADING) {
        log_d("Ignoring loading message update: dismissed=%d, state=%d", dismissed_,
              static_cast<int>(state_));
        return;
    }

    log_i("Setting loading message: %s", message.c_str());
    questionView_.setText(message);
    descriptionView_.setText("");
    setNeedsRerender();

    // Make sure we don't lose the renderer stream when updating the loading message
    auto savedStream = rendererStream_;

    // Restore the saved stream if it was replaced during render
    if (savedStream != nullptr && rendererStream_ == nullptr) {
        log_d("Restoring renderer stream that was lost during loading message update");
        rendererStream_ = savedStream;
    }
}

void BookQuestionsScreen::reset() {
    log_d("Resetting BookQuestionsScreen");

    // Reset question response and pagination state
    // Clear the answer string to free memory
    questionResponse_.answer.clear();
    questionResponse_.error.clear();
    questionResponse_.success = false;
    questionResponse_.tokensUsed = 0;
    questionResponse_.cost = 0.0f;

    // Reset pagination state
    currentPage_ = 0;
    answerPlace_ = std::dynamic_pointer_cast<TxtPlace>(Place::factory(0));
    precomputedTotalPages_ = 0;
    pageBreakOffsets_.clear();
    pageNumbers_.clear();

    // Reset selected model
    selectedModelId_.clear();
    selectedModelIndex_ = 0;

    // Reset dismissed flag
    dismissed_ = false;

    // Clear any existing renderer stream
    if (rendererStream_ != nullptr) {
        log_d("Resetting renderer stream in reset()");
        auto streamToRestore = rendererStream_;
        rendererStream_ = nullptr;
        renderer_.setStream(streamToRestore);
    }

    // Reset to questions list state
    state_ = State::QUESTIONS_LIST;
    updateBackLabel();
    updateUIForState();

    // Reset menu selection
    menuView_.setSelectedIndex(0);

    // Ensure we reload menu items
    needsMenuItemsLoad_ = true;

    // Set first load flag
    setFirstLoad();

    // Clear the display to ensure no remnants remain
    display_.clearDisplay();
}
