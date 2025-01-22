BUILD_TYPE := Debug

.PHONY: configure-iphoneos
configure-iphoneos:
	cmake -G Xcode -B build-iphoneos -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_OSX_SYSROOT=iphoneos .

.PHONY: build-iphoneos
build-iphoneos: configure-iphoneos
	cmake --build build-iphoneos --config $(BUILD_TYPE) --target SolSimIos

.PHONY: configure-iphonesimulator
configure-iphonesimulator:
	cmake -G Xcode -B build-iphonesimulator -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_OSX_SYSROOT=iphonesimulator .

.PHONY: build-iphonesimulator
build-iphonesimulator: configure-iphonesimulator
	cmake --build build-iphonesimulator --config $(BUILD_TYPE) --target SolSimIos

.PHONY: build
build: build-iphoneos build-iphonesimulator
	rm -rf build/SolSimIos.xcframework
	xcodebuild -create-xcframework \
    -framework build-iphoneos/$(BUILD_TYPE)-iphoneos/SolSimIos.framework \
    -framework build-iphonesimulator/$(BUILD_TYPE)-iphonesimulator/SolSimIos.framework \
    -output build/SolSimIos.xcframework

.PHONY: clean
clean:
	rm -rf build build-iphoneos build-iphonesimulator
