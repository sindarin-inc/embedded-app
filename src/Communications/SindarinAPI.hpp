#pragma once
#include <ArduinoJson.h>
#include <esp_http_client.h>
#include <functional>
#include <utility>

#include "Communications/HttpClient.hpp"
#include "Misc/CancelFlag.hpp"
#include "Misc/Telemetry.hpp"
#include "Models/BookQuestionRequest.hpp"
#include "Models/CheckSync.hpp"
#include "Models/Firmware.hpp"
#include "Models/FirmwareResponse.hpp"
#include "Models/Json.hpp"
#include "Models/KindlePageResponse.hpp"
#include "Models/ReadableArchive.hpp"
#include "Models/ReadableBookmark.hpp"
#include "Models/ReadablePlacemark.hpp"
#include "Models/SyncBookmarksResponse.hpp"
#include "Models/UserReadable.hpp"
#include "Storage/FileSystem.hpp"
#include "Storage/PreferencesStore.hpp"

enum SindarinError {
    ERR_SET_AUTH = -1,
    ERR_CLIENT_OPEN = -2,
    ERR_CLIENT_WRITE = -3,
    ERR_CONTENT_LENGTH = -4,
    ERR_JSON_DESERIALIZE = -5,
    ERR_JSON_OBJECT_PARSE = -6,
    ERR_SET_HEADER = -7,
    ERR_CLIENT_DISCONNECTED = -8,
    // Used in FileDownload
    ERR_OPEN_BLOCK = -9,
    ERR_SIZE_MISMATCH = -10,
    ERR_FILE_OPEN = -11,
    ERR_ALLOCATION = -12,
    ERR_CANCELLED = -13,
    ERR_READ_EMPTY = -14,
    ERR_BLE_NOT_CONNECTED = -15,
    ERR_BLE_TIMED_OUT = -16,
    ERR_CREATE_DIR = -17,
    ERR_CLIENT_READ = -18,
    ERR_INVALID_PARAM = -19,
    ERR_EXCEPTION = -20,
};

class SindarinAPI {
public:
    SindarinAPI(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}
    SindarinAPI(SindarinAPI const &) = delete;

    static auto getInstance() -> std::shared_ptr<SindarinAPI>;

    void overrideBaseUrl(std::string baseUrl) { baseUrl_ = std::move(baseUrl); }
    auto isSindarinUrl(const char *url) -> bool;
    void setAccessToken(const std::string &accessToken);

    auto checkSync(CheckSyncRequest &req, CheckSyncResponse *resp, Telemetry &telemetry,
                   bool useBle, const std::shared_ptr<CancelFlag> &cancel) -> int;
    auto checkSync(const std::string &url, CheckSyncRequest &req, CheckSyncResponse *resp,
                   Telemetry &telemetry, bool useBle, const std::shared_ptr<CancelFlag> &cancel)
        -> int;
    auto checkFirmware(FirmwareResponse *resp, bool useBle,
                       const std::shared_ptr<CancelFlag> &cancel) -> int;

    auto downloadReadable(const std::shared_ptr<Readable> &readable, FileSystem &fileSystem,
                          Telemetry &telemetry, const std::string &path,
                          const std::shared_ptr<CancelFlag> &cancel, bool useBle,
                          const std::string &publicKey,
                          const std::function<void(int percentage)> &progressCallback) -> int;
    static auto createClient(const std::string &url, const std::string &contentType,
                             esp_http_client_method_t method, bool useBle,
                             const std::shared_ptr<CancelFlag> &cancel, int timeoutMs = 30000)
        -> std::shared_ptr<HttpClient>;
    auto uploadTelemetryFile(SFile &telemetryFile, const std::string &localPath,
                             FileSystem &fileSystem, bool useBle = false,
                             const std::shared_ptr<CancelFlag> &cancel = nullptr) -> int;
    auto uploadCrashReport(SFile &crashFile, const std::string &contentType) -> int;

    auto setAuth(const std::shared_ptr<HttpClient> &client) -> esp_err_t;

    auto getServerTime(time_t *time, bool useBle = false,
                       const std::shared_ptr<CancelFlag> &cancel = nullptr) -> int;

    auto askQuestion(const BookQuestionRequest &req, BookQuestionResponse *resp, bool useBle,
                     const std::shared_ptr<CancelFlag> &cancel) -> int;

    // Kindle book operations
    auto openKindleBook(const SpiramString &title, KindlePageResponse *resp,
                        const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle) -> int;
    auto navigateKindle(KindlePageResponse *resp, const std::shared_ptr<CancelFlag> &cancelFlag,
                        bool useBle, int navigate, int preview) -> int;

    // Submit user's choice for last read page dialog
    auto submitLastReadPageChoice(bool gotoLastReadPage, bool useBle) -> int;

private:
    auto tryCheckFirmware(FirmwareResponse *resp, bool useHttp, bool useBle,
                          const std::shared_ptr<CancelFlag> &cancel) -> int;
    auto makeKindleRequest(const std::string &path, KindlePageResponse *resp,
                           const std::shared_ptr<CancelFlag> &cancelFlag, bool useBle) -> int;

protected:
    static std::shared_ptr<SindarinAPI> instance;
    std::string baseUrl_;
    std::string accessToken_;
};
