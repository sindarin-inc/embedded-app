#ifndef ONBOARDING_EPUB_MANAGER_HPP
#define ONBOARDING_EPUB_MANAGER_HPP

#include <string>
#include <utility>
#include <vector>

#include "OnboardingEpub1.hpp"

class OnboardingEpub {
public:
    OnboardingEpub(std::string name, const unsigned char *bytes, const size_t size)
        : name(std::move(std::move(name))), bytes(bytes), size(size){};
    ~OnboardingEpub() = default;
    ;

    std::string name;
    const unsigned char *bytes;
    size_t size;
};

class OnboardingEpubManager {
public:
    OnboardingEpubManager() {
        OnboardingEpub first("Onboarding One", ONBOARDING_EPUB1, ONBOARDING_EPUB1_SIZE);
        onboardingEpubsVector.push_back(first);
        // duplicated on purpose for demonstration
        OnboardingEpub second("Onboarding Two", ONBOARDING_EPUB1, ONBOARDING_EPUB1_SIZE);
        onboardingEpubsVector.push_back(second);
        onboardingEpubsVector.emplace_back("Onboarding Three", ONBOARDING_EPUB1,
                                           ONBOARDING_EPUB1_SIZE);
    }
    ~OnboardingEpubManager() = default;

    auto find(const std::string &name) -> OnboardingEpub {
        for (const auto &epub : onboardingEpubsVector) {
            if (epub.name == name) {
                return epub;
            }
        }
        OnboardingEpub notFound("not found", nullptr, 0);
        return notFound;
    }

    std::vector<OnboardingEpub> onboardingEpubsVector;
};

#endif