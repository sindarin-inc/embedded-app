#ifndef ONBOARDING_EPUB_MANAGER_HPP
#define ONBOARDING_EPUB_MANAGER_HPP

#include <string>
#include <vector>

#include "OnboardingEpub1.hpp"

class OnboardingEpub {
public:
    OnboardingEpub(std::string name, const unsigned char *bytes, const size_t size)
        : name_(name), bytes_(bytes), size_(size){};
    ~OnboardingEpub(){};

    std::string name_;
    const unsigned char *bytes_;
    size_t size_;
};

class OnboardingEpubManager {
public:
    OnboardingEpubManager() {
        OnboardingEpub first("Onboarding One", onboardingEpub1, onboardingEpub1Size);
        onboardingEpubsVector.push_back(first);
        // duplicated on purpose for demonstration
        OnboardingEpub second("Onboarding Two", onboardingEpub1, onboardingEpub1Size);
        onboardingEpubsVector.push_back(second);
        onboardingEpubsVector.push_back({"Onboarding Three", onboardingEpub1, onboardingEpub1Size});
    }
    ~OnboardingEpubManager() {}

    auto find(std::string name) -> OnboardingEpub {
        for (const auto &epub : onboardingEpubsVector) {
            if (epub.name_ == name) {
                return epub;
            }
        }
        OnboardingEpub notFound("not found", nullptr, 0);
        return notFound;
    }

    std::vector<OnboardingEpub> onboardingEpubsVector;
};

#endif