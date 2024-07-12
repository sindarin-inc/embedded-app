#include "WiFiScanScreen.hpp"

#include <unordered_map>

void WiFiScanScreen::begin() {
    DispatchGetCore0BackgroundQueue()->dispatch([this]() {
        std::vector<wifi_ap_record_t> networks;

        wifi_.scanNetworks(
            [&networks](const std::shared_ptr<wifi_ap_record_t[]> &apRecords, int recordCount) {
            networks.insert(networks.end(), apRecords.get(), apRecords.get() + recordCount);
        });
        // Shut down and deinit everything WiFi
        wifi_.disconnectWiFi();

        std::unordered_map<std::string, wifi_ap_record_t> uniqueNetworks;
        for (auto &network : networks) {
            std::string ssid(reinterpret_cast<const char *>(network.ssid));
            if (uniqueNetworks.find(ssid) == uniqueNetworks.end() ||
                network.rssi > uniqueNetworks[ssid].rssi) {
                uniqueNetworks[ssid] = network;
            }
        }

        log_d("Done scanning networks, %zu aps found (%zu unique)", networks.size(),
              uniqueNetworks.size());

        // Sort uniqueNetworks by signal strength
        std::vector<wifi_ap_record_t> sortedNetworks;
        sortedNetworks.reserve(uniqueNetworks.size());
        for (auto &pair : uniqueNetworks) {
            sortedNetworks.push_back(pair.second);
        }
        std::sort(sortedNetworks.begin(), sortedNetworks.end(),
                  [](const wifi_ap_record_t &a, const wifi_ap_record_t &b) {
            return a.rssi > b.rssi;
        });

        DispatchGetUiQueue()->dispatch([this, sortedNetworks]() {
            networkViews_.clear();
            if (sortedNetworks.empty()) {
                statusView_.setText("No networks found");
                statusView_.setHidden(false);
            } else {
                statusView_.setHidden(true);
                for (auto &network : sortedNetworks) {
                    std::string displayText = StringFormat(
                        "%s (%d dBm)", reinterpret_cast<const char *>(network.ssid), network.rssi);
                    networkViews_.emplace_back(display_, displayText, &ibmf75Face0);
                }
            }
            render(display_);
        });
    });
}

auto WiFiScanScreen::layout(LayoutContext &context, Size size) -> Size {
    context.setViewFrame(statusView_, {{0, 0}, size},
                         LayoutOptionsSizeToFit | LayoutOptionsAlignCenter);

    int y = 0;
    for (auto &view : networkViews_) {
        auto textRect =
            context.setViewFrame(view, {{-10, y}, size}, LayoutOptionsSizeToFitVertical);
        y += textRect.size.height + 2;
    }
    return size;
}

void WiFiScanScreen::drawInRect(Rect rect) {
    for (auto &view : networkViews_) {
        view.draw();
    }
    if (networkViews_.empty()) {
        statusView_.draw();
    }
}

void WiFiScanScreen::render(DisplaySystem &display) {
    setNeedsLayout();
    setFrame(defaultFrameRect());
    draw();
    display.updateDisplay();
}

void WiFiScanScreen::processCommand(RemoteCommand command) {
    // All commands just go back
    back();
};
