import logging
import termios
import time
import typing as t
from typing import Any
import platform
import os
import csv

import pytest
import serial
from _pytest.fixtures import FixtureRequest
from pytest import MonkeyPatch
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.serial import IdfSerial
from pytest_embedded.log import live_print_call

from ict import MFICTClient
from power import PowerTester
from util import maybe_format, wait_for_port_by_device_name

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

dut_port_name = "USB JTAG/serial debug unit"
dut_port_index = 0

dut_log_port_name = "SOL Mainframe ICT"
dut_log_port_index = 1


@pytest.fixture
def port(request: FixtureRequest) -> t.Optional[str]:
    """Overridden to make sure we pick the right USB port for the DUT"""
    custom_port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
    return custom_port


class OverrideIdfSerial(IdfSerial):
    def __init__(self, ict_client: MFICTClient, *args: Any, **kwargs: Any) -> None:
        self.ict_client = ict_client
        super().__init__(*args, **kwargs)

    def _start(self):
        # Wake up from sleep if necessary
        self.ict_client.set_servo("right", -70)

        # The DUT is flashed here unless using --skip-autoflash=True
        super()._start()

        # read ict's bluetooth mac
        btmac = self.ict_client.get_bt_mac()
        logger.info("ICT bluetooth MAC: " + btmac)

        wifi_ssid = self.ict_client.get_wifi_ssid()
        logger.info("ICT ssid: " + wifi_ssid)

        wifi_password = self.ict_client.get_wifi_password()
        logger.info("ICT password: " + wifi_password)

        # prepare nvs partition image and write to dut
        self.write_nvs(btmac, wifi_ssid, wifi_password)

        # Close the serial port
        self.close()

        # Reset the DUT (to exit programming mode)
        self.ict_client.reset_dut()

        # Wait for device to go away and come back again
        # If we don't do this, we may discover and attempt to connect to the port too quickly, then it will de-enumerate.
        time.sleep(2)
        port = wait_for_port_by_device_name(dut_port_name, dut_port_index)

        self.connect_dut_log_port()

    def get_nvs_partition_size(self):
        file_path = "esp32s3-64MB.csv"
        nvs_size = ""
        # Correct code to extract size of 'nvs' partition
        with open(file_path, "r") as file:
            csv_reader = csv.reader(file, delimiter=",")
            for row in csv_reader:
                if row[0] == "nvs":
                    nvs_size = row[4]

        # Convert hexadecimal to decimal
        nvs_size_decimal = int(nvs_size, 16)
        return nvs_size_decimal

    def write_nvs(self, btmac, ict_ssid, ict_pw):
        parts = btmac.split(":")
        hexstr = "".join(parts)
        logger.info("hex string: " + hexstr)
        btmac_int = int(hexstr, 16)
        logger.info(str(btmac_int))

        # create partition csv file
        csvfname = "dutnvs.csv"
        fp = open(csvfname, "w")
        fp.write("key,type,encoding,value\n")
        fp.write("boot-info,namespace,,\n")
        fp.write("sindarin-app,namespace,,\n")
        # write paired tt address
        fp.write("paired-tt,data,u64," + str(btmac_int) + "\n")
        fp.write("no-setup,data,u8,1\n");
        # Write a cloud-token so the glasses don't try to go back into ship mode upon sleep
        fp.write("cloud-token,data,string,FAKECLOUDTOKEN\n")

        wifi_settings_json = "\"{\"\"settings\"\":[{\"\"ssid\"\":\"\"" + ict_ssid + "\"\",\"\"password\"\":\"\"" + ict_pw + "\"\"}]}\""

        fp.write("wifi-settings,data,string," + wifi_settings_json + "\n")
        # Skip the onboarding thing since it has a bunch of animations
        fp.write("no-setup,data,u8,1\n")
        fp.close()

        # check for $IDF_PATH variable
        if "IDF_PATH" not in os.environ:
            # no idf path set, must be running from an exported test package
            # set the variable so tools can be found
            os.environ["IDF_PATH"] = "test/PyTest/vendor"

        # create partition bin file
        binfname = "dutnvs.bin"
        nvssize = self.get_nvs_partition_size()
        logger.info("nvs partition size: " + str(nvssize))

        live_print_call(
            [
                os.environ["IDF_PATH"] + "/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py",
                "generate",
                csvfname,
                binfname,
                str(nvssize),
            ],
            msg_queue=self._q,
        )

        # write nvs partition to dut
        live_print_call(
            [
                os.environ["IDF_PATH"] + "/components/partition_table/parttool.py",
                "--port",
                self.port,
                "write_partition",
                "--partition-name=nvs",
                "--input",
                binfname,
            ],
            msg_queue=self._q,
        )

        os.remove(csvfname)
        os.remove(binfname)

    def connect_dut_log_port(self):
        max_attempts = 3
        for n in range(max_attempts):
            # self.close()
            # find second device with SOL Mainframe ICT name
            # this is the DUT serial relay
            port = wait_for_port_by_device_name(dut_log_port_name, dut_log_port_index)
            self.proc.portstr = port
            logger.debug(f"Found port {port} for DUT serial port, re-opening")
            try:
                self.proc.open()
            except (serial.serialutil.SerialException, termios.error) as e:
                logger.warning(f"Attempt {n+1} failed with error: {e}")
                time.sleep(1)
            except BrokenPipeError as e:
                # For whatever reason configuring rtscts/dsrdtr crash at self.proc.open() in linux with a trace like this:
                # venv/lib/python3.10/site-packages/serial/serialutil.py:463: in rts
                #     self._update_rts_state()
                # venv/lib/python3.10/site-packages/serial/serialposix.py:708: in _update_rts_state
                #     fcntl.ioctl(self.fd, TIOCMBIC, TIOCM_RTS_str)
                # E   BrokenPipeError: [Errno 32] Broken pipe
                # As a workaround we force them to True and try again. Not sure if that's good
                if platform.system() == "Linux":
                    logger.warning("Attempting workaround for Linux rtscts/dsrdtr BrokenPipeError")
                    self.proc.rtscts = True
                    self.proc.dsrdtr = True
                else:
                    raise e
            else:
                logger.debug("Reopened DUT serial port successfully")
                break
        else:
            raise serial.serialutil.SerialException("Unable to reconnect DUT serial")
        self._post_init()
        self._finalize_init()
        self.start_redirect_thread()


class OverrideDut(IdfDut):
    """Overridden to make sure we pick the right USB port for the DUT"""

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
        kwargs["port"] = port
        super().__init__(*args, **kwargs)


@pytest.fixture(scope="module")
def monkeypatch_module(request: FixtureRequest) -> MonkeyPatch:
    mp = MonkeyPatch()
    request.addfinalizer(mp.undo)
    return mp


@pytest.fixture(scope="module", autouse=True)
def replace_dut_class(monkeypatch_module: MonkeyPatch) -> None:
    """Subclass the IdfDut fixture"""
    monkeypatch_module.setattr("pytest_embedded_idf.IdfDut", OverrideDut)


@pytest.fixture(scope="module", autouse=True)
def replace_serial_class(monkeypatch_module: MonkeyPatch, ict_client: MFICTClient) -> None:
    """Subclass the IdfSerial fixture"""

    def override_idf_serial_factory(ict_client):
        def _constructor(*args: Any, **kwargs: Any):
            return OverrideIdfSerial(ict_client, *args, **kwargs)

        return _constructor

    monkeypatch_module.setattr("pytest_embedded_idf.IdfSerial", override_idf_serial_factory(ict_client))


@pytest.fixture(scope="session")
def ict_client():
    # Set everything up to be discoverable over USB
    client = MFICTClient()
    client.clear_input()
    client.set_gpio0(0)
    client.set_power(True)
    client.reset_dut()
    client.set_gpio0(1)

    wait_for_port_by_device_name(dut_port_name, dut_port_index)

    yield client  # Tests run here

    logger.debug("Tearing down MFICTClient")

    # Reset everything back to a default state
    client.set_servo("right", 0)
    client.set_servo("left", 0)
    client.set_power(False)


@pytest.fixture(scope="session")
def power_client():
    client = PowerTester()
    yield client
    client.turn_off()


@pytest.mark.usefixtures("ict_client")
@pytest.mark.ict
@pytest.mark.skip(reason="Does not work because ActiveUSBCharging blocks it")
class TestWithIctOnly:
    """Tests that only require the ICT to be connected (and not the power profiler)"""

    def test_power_on(self, dut: IdfDut, ict_client: MFICTClient) -> None:
        # Note: If the IdfDut tries to connect to the ICT serial port to flash it, it dumps a bunch of serial
        # garbage to the ICT that will cause it to fail the next command. This clears the input buffer by sending a newline.
        ict_client.clear_input()

        # Check 3.3V supply voltage
        voltage = ict_client.get_voltage("V33")
        assert 3.2 < voltage < 3.4

        # Check charger voltages
        assert 3.7 < ict_client.get_voltage("VLB") < 4.3
        assert 3.7 < ict_client.get_voltage("VRB") < 4.3

        # NOTE: Currently we don't do a full boot when USB power is connected, so we can't get any further than this

        # maybe_format(dut)

        # dut.expect("Sol: Error-free boot!", timeout=30)

        # # Close right temple and make sure we go to sleep
        # ict_client.set_servo("right", 0)

        # dut.expect("Sol: Temple is closed", timeout=10)
        # dut.expect("Sol: Entering deep sleep, wake up with magnetometer", timeout=10)


@pytest.mark.usefixtures("ict_client")
@pytest.mark.usefixtures("power_client")
@pytest.mark.ict
@pytest.mark.power
class TestPowerConsumption:
    def test_power(self, dut: OverrideDut, ict_client: MFICTClient, power_client: PowerTester) -> None:
        # Serial comms go down here when we're toggling power. Go ahead and close out the connection.
        dut.serial.close()

        logger.info("Testing voltage rails")

        # Check 3.3V supply voltage
        voltage = ict_client.get_voltage("V33")
        assert 3.2 < voltage < 3.4

        # Check charger voltages
        assert 3.7 < ict_client.get_voltage("VLB") < 4.3
        assert 3.7 < ict_client.get_voltage("VRB") < 4.3

        # Note: If the IdfDut tries to connect to the ICT serial port to flash it, it dumps a bunch of serial
        # garbage to the ICT that will cause it to fail the next command. This clears the input buffer by sending a newline.
        ict_client.clear_input()

        ict_client.set_power(False)
        ict_client.set_servo("left", 0)

        power_client.turn_on()

        time.sleep(0.5)

        # Test ship mode current
        logger.info("Measuring ship mode current")
        power_client.expect_current((10, 30), 3)

        logger.info("Powering on DUT")
        ict_client.set_servo("right", -70)
        time.sleep(0.5)
        ict_client.set_servo("left", 75)

        # Wait for serial port to come back
        # first wait for DUT device to show up
        port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
        # then connect to serial relay for log parsing
        dut.serial.connect_dut_log_port()

        time.sleep(2)

        # Check voltage while powered via the battery connector (tests the battery combiner circuit)
        voltage = ict_client.get_voltage("V33")
        assert 3.2 < voltage < 3.4

        # NOTE NOTE NOTE NOTE WARNING: Currently this will only work if the firmware is compiled with -DDISABLE_POWER_MANAGEMENT=1.
        # That's because without USB power connected, the DUT will sporadically go into light sleep here, which will cause the USB serial to come and go.
        # One potential workaround would be to proxy all the serial logs through the ICT, but that will require some extra work, since we'll have to rewire the `expect` commands to use the DUT's serial port.

        maybe_format(dut)

        dut.expect("Sol: Error-free boot!", timeout=30)

        # wait for ICT pageturner connect
        dut.expect("Connected to PageTurner", timeout=20)

        time.sleep(3)

        logger.info("Measuring active mode current")
        power_client.expect_current((7000, 13000), 5)

        # wait 5 seconds
        time.sleep(5)
        
        # turntouch button
        ict_client.tt_enter()

        # wait for wifi connection
        dut.expect("Connected, IP address", timeout=30)

        logger.info("Sleeping")
        ict_client.set_servo("right", 0)

        dut.expect("Sol: Temple is closed", timeout=10)
        # Macs for some reason don't get as many of the logs as Linux. Maybe a PySerial bug?
        if platform.system() == "Darwin":
            dut.expect("Sol: deepSleepInternal with", timeout=20)
        else:
            dut.expect("Sol: Sleeping for", timeout=20)

        time.sleep(5)

        logger.info("Measuring deep sleep current")
        power_client.expect_current((50, 100), 5)
