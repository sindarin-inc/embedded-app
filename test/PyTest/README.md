# PyTest Test Suite

The PyTest test suite allows us to run tests on real hardware. Using Espressif's [pytest-embedded](https://github.com/espressif/pytest-embedded) it has support for things like automatically flashing firmware and watching serial output to match certain regexes.

We've added support for the Nordic PPK2 and our in-house ICT tester board as well.

The ICT needs to be programmed with its own firmware available [here](https://github.com/sindarin-inc/mainframe-tester).

## Generate a test package

This includes the firmware, test scripts, and any other files needed to run the tests without needing to have a copy of esp-idf or our source code. This is what we'll give to manufacturers and other third parties to test boards/glasses. From the root of the repo run:

```sh
./test/PyTest/generate_test_package.sh
```

Now you can probably compress up that dir and run it on another computer.

## Visualizing PPK2 Data

There's a Plotly html file that can graph the exported PPK2 data from power tests.

After running tests copy in the `current_samples_*` files into `test/Pytest` dir. When running remotely you might do something like this:

```sh
scp sol@192.168.1.235:~/Public/sol-pytest-5c612fd7/current_samples_0-10-30.csv ./test/Pytest/
scp sol@192.168.1.235:~/Public/sol-pytest-5c612fd7/current_samples_1-30000-110000.csv ./test/Pytest/
scp sol@192.168.1.235:~/Public/sol-pytest-5c612fd7/current_samples_2-150-250.csv ./test/Pytest/
```

Then run a Python HTTP server.

```sh
python3 -m http.server --directory test/PyTest
```

And in a separate terminal open up the plots

```sh
open http://localhost:8000/plotly.html?csv=current_samples_0-10-30.csv
open http://localhost:8000/plotly.html?csv=current_samples_1-30000-110000.csv
open http://localhost:8000/plotly.html?csv=current_samples_2-150-250.csv
```

## TODO

* Set this up to run from CI
* Proxy logs through the ICT serial (so this works with light sleep -- which gives us a better active mode current measurement)
* Add support for BLE and WiFi testing from the ICT
