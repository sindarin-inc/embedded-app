# PyTest Test Suite

The PyTest test suite allows us to run tests on real hardware. Using Espressif's [pytest-embedded](https://github.com/espressif/pytest-embedded) it has support for things like automatically flashing firmware and watching serial output to match certain regexes.

We've added support for the Nordic PPK2 and our in-house ICT tester board as well.

The ICT needs to be programmed with its own firmware available [here](https://github.com/sindarin-inc/mainframe-tester).

## Generate a test package

This includes the firmware, test scripts, and any other files needed to run the tests without needing to have a copy of esp-idf or our source code. This is what we'll give to manufacturers and other third parties to test boards/glasses. From the root of the repo run:

```sh
idf.py fullclean
BUILD_FOR_TEST=1 idf.py set-target esp32s3
BUILD_FOR_TEST=1 idf.py build

export GITSHA=$(git log -1 --format=%h)
export VERSION=sol-pytest-$GITSHA
export OUTDIR=$VERSION
mkdir $OUTDIR

# Copy necessary things into `build`
mkdir $OUTDIR/build
mkdir $OUTDIR/build/bootloader
cp build/bootloader/bootloader.bin $OUTDIR/build/bootloader/
mkdir $OUTDIR/build/config
cp build/config/sdkconfig.json $OUTDIR/build/config/
cp build/embedded-app.bin $OUTDIR/build/
cp build/embedded-app.elf $OUTDIR/build/
cp build/flasher_args.json $OUTDIR/build/
cp build/ota_data_initial.bin $OUTDIR/build/
mkdir $OUTDIR/build/partition_table
cp build/partition_table/partition-table.bin $OUTDIR/build/partition_table/

cp pytest.ini $OUTDIR/
mkdir -p $OUTDIR/test/PyTest
cp test/PyTest/*.py $OUTDIR/test/PyTest/
pip3 install ppk2-api
pip freeze > $OUTDIR/requirements.txt

cat << EOF > $OUTDIR/README.md
# Sindarin's PyTest Test Suite

## Setting up the test environment

Generally you'll need Python3 with Pip installed in order to run tests.

    python3 -m venv venv
    source venv/bin/activate
    pip install -r requirements.txt

Then you can probably just run the tests with pytest:

    pytest

If you use \`espeak\` on Linux, or \`say\` on macOS, you can have the test suite speak the results to you:

    pytest && espeak pass || espeak fail

    pytest && say -v "Good News" pass || say -v "Bad News" fail

The \`--skip-autoflash=True\` might also come in handy if you are running the test suite mutiple times on the same board. It allows you to skip programming the device under test (DUT).

    pytest --skip-autoflash=True

### Ubuntu-specific provisioning

Based on a fresh Ubuntu 22.04 install, you'll need to install some packages before running the above:

    sudo apt install python3.10-venv python3-pip espeak
    sudo usermod -a -G dialout $USER
    sudo reboot

EOF
```

Now you can probably compress up that dir and run it on another computer.

## TODO

* Set this up to run from CI
* Proxy logs through the ICT serial (so this works with light sleep -- which gives us a better active mode current measurement)
* Add support for BLE and WiFi testing from the ICT
