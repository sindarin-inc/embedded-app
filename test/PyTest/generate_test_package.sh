#!/usr/bin/env bash
# Should be run from the root of the embedded-app repo

# Abort on failure
set -e

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
pip3 install ppk2-api==0.9.2
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
