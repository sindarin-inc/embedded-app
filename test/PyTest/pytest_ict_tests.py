import csv
import logging
import os
import platform
import subprocess
import sys
import termios
import time
import typing as t
from typing import Any

import pytest
import serial
from _pytest.fixtures import FixtureRequest
from power import PowerTester
from pytest import MonkeyPatch
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_idf.serial import IdfSerial
from util import maybe_format, wait_for_port_by_device_name

from ict import MODE_FINAL_TESTER, MODE_MAINFRAME_TESTER, MFICTClient

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

dut_port_name = "USB JTAG/serial debug unit"
dut_port_index = 0

dut_log_port_name = "SOL Mainframe ICT"
dut_log_port_index = 1

test_user_cloud_token = "c7092bfc68fac544fbe46e75ef06948b867a4854ce4271c2f9fea16abfd20125"


def get_nvs_partition_size():
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


def clear_nvs(port):
    logger.info("Clearing NVS")
    # write nvs partition to dut
    subprocess.run(
        [
            os.environ["IDF_PATH"] + "/components/partition_table/parttool.py",
            "--port",
            port,
            "erase_partition",
            "--partition-name=nvs",
        ]
    )


def write_nvs(ict_client, port, btmac, ict_ssid, ict_pw, cloudtoken):
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
    fp.write("no-setup,data,u8,1\n")
    # Write a cloud-token so the glasses don't try to go back into ship mode upon sleep
    fp.write("cloud-token,data,string," + cloudtoken + "\n")

    wifi_settings_json = '"{""settings"":[{""ssid"":""' + ict_ssid + '"",""password"":""' + ict_pw + '""}]}"'

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
    nvssize = get_nvs_partition_size()
    logger.info("nvs partition size: " + str(nvssize))

    ret = subprocess.run(
        [
            os.environ["IDF_PATH"] + "/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py",
            "generate",
            csvfname,
            binfname,
            str(nvssize),
        ]
    )

    logger.info("generate partition return value " + str(ret.returncode))
    ret.check_returncode()

    parts = [
        os.environ["IDF_PATH"] + "/components/partition_table/parttool.py",
        "--port",
        port,
        "write_partition",
        "--partition-name=nvs",
        "--input",
        binfname,
    ]

    print("command:")
    print(" ".join(parts))

    # write nvs partition to dut
    ret = subprocess.run(parts)

    max_retries = 5
    retry = 0
    while ret.returncode != 0 and retry < max_retries:
        logger.info("retry")
        retry += 1
        ict_client.dut_programming_mode()
        # look for jtag port
        time.sleep(2)
        port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
        logger.info("nvs retry dut port: " + port)
        parts = [
            os.environ["IDF_PATH"] + "/components/partition_table/parttool.py",
            "--port",
            port,
            "write_partition",
            "--partition-name=nvs",
            "--input",
            binfname,
        ]
        print("command:")
        print(" ".join(parts))
        # write nvs partition to dut
        ret = subprocess.run(parts)

    os.remove(csvfname)
    os.remove(binfname)

    ret.check_returncode()


def clear_content_and_restart(ict_client):
    # go to system menu, clear all content and restart
    # ensure that actual downloads happen during the content sync
    ict_client.tt_left()
    time.sleep(1.0)
    ict_client.tt_down()
    time.sleep(1.0)
    ict_client.tt_down()
    time.sleep(1.0)
    ict_client.tt_enter()
    time.sleep(1.0)
    ict_client.tt_up()
    time.sleep(1.0)
    ict_client.tt_up()
    time.sleep(1.0)
    ict_client.tt_up()
    time.sleep(1.0)
    ict_client.tt_enter()


def generate_and_write_nvs(ict_client, dut, ssid, password):
    # write nvs
    # read ict's bluetooth mac
    btmac = ict_client.get_bt_mac()
    logger.info("ICT bluetooth MAC: " + btmac)

    wifi_ssid = ict_client.get_wifi_ssid()
    wifi_password = ict_client.get_wifi_password()

    if ssid != "" and password != "":
        wifi_ssid = ssid
        wifi_password = password

    logger.info("params user " + wifi_ssid + " password " + wifi_password)

    # look for jtag port
    time.sleep(2)
    port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
    logger.info("nvs dut port: " + port)

    logger.info("write nvs")
    write_nvs(ict_client, port, btmac, wifi_ssid, wifi_password, test_user_cloud_token)

    logger.info("reset")
    logger.info("exiting programming mode")
    ict_client.reset_dut()

    time.sleep(2)
    port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
    logger.info("post program nvs dut port: " + port)


@pytest.fixture
def port(request: FixtureRequest) -> t.Optional[str]:
    """Overridden to make sure we pick the right USB port for the DUT"""
    custom_port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
    logger.info("custom port: " + custom_port)
    return custom_port


class OverrideIdfSerial(IdfSerial):
    def __init__(self, ict_client: MFICTClient, *args: Any, **kwargs: Any) -> None:
        self.ict_client = ict_client
        ict_mode = self.ict_client.get_ict_mode()

        if ict_mode == MODE_FINAL_TESTER:
            logger.info("opening temple")
            self.ict_client.open_right_temple()
            self.ict_client.dut_programming_mode()

        s = False
        while s == False:
            try:
                s = True
                super().__init__(*args, **kwargs)
            except Exception as e:
                logger.info("failed to open dut port, retrying")
                s = False

                logger.info("dut going into programming mode")
                self.ict_client.dut_programming_mode()
                port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
                logger.info("found dut port: " + port)
                kwargs["port"] = port
                logger.info("retrying connect to " + port)

    def _start(self):
        ict_mode = self.ict_client.get_ict_mode()
        logger.info("start ict mode " + str(ict_mode))

        # Wake up from sleep if necessary
        self.ict_client.open_right_temple()

        # The DUT is flashed here unless using --skip-autoflash=True
        super()._start()

        # Close the serial port
        self.close()

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
        logger.info("done init idfdut")


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
    if client.get_ict_mode() == MODE_FINAL_TESTER:
        logger.info("ict client opening dut temple")
        client.open_right_temple()

    client.set_gpio0(0)
    client.set_power(True)
    client.reset_dut()
    client.set_gpio0(1)

    wait_for_port_by_device_name(dut_port_name, dut_port_index)

    yield client  # Tests run here

    logger.debug("Tearing down MFICTClient")

    ict_mode = client.get_ict_mode()

    # Reset everything back to a default state
    client.open_right_temple()
    client.open_left_temple()
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

    def test_power_on(self, dut: IdfDut, ict_client: MFICTClient, ssid) -> None:
        logger.info("power on ssid " + ssid)

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


# Power Consumption test
# only supported on mainframe tester
@pytest.mark.usefixtures("ict_client")
@pytest.mark.usefixtures("power_client")
@pytest.mark.ict
@pytest.mark.power
@pytest.mark.skipif(MFICTClient().get_ict_mode() != MODE_MAINFRAME_TESTER, reason="not mainframe tester")
class TestPowerConsumption:
    def test_power(self, dut: OverrideDut, ict_client: MFICTClient, power_client: PowerTester) -> None:
        generate_and_write_nvs(ict_client, dut, "", "")

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
        ict_client.close_left_temple()

        power_client.turn_on()

        time.sleep(0.5)

        # Test ship mode current
        logger.info("Measuring ship mode current")
        power_client.expect_current((10, 30), 3)

        logger.info("Powering on DUT")
        ict_client.open_right_temple()
        time.sleep(0.5)
        ict_client.open_left_temple()

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

        # clear content, ensure next button press triggers content sync
        # this will reset the dut
        clear_content_and_restart(ict_client)

        # wait for boot again
        dut.expect("Sol: Error-free boot!", timeout=30)

        # wait for ICT pageturner connect
        dut.expect("Connected to PageTurner", timeout=20)
        logger.info("got pageturner message")

        time.sleep(5.0)

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


def perform_sync(dut, ict_client, ssid, password):
    full = True
    if ssid == "" and password == "":
        full = False

    ict_client.open_right_temple()
    ict_client.open_left_temple()

    # Wait for serial port to come back
    # first wait for DUT device to show up
    port = wait_for_port_by_device_name(dut_port_name, dut_port_index)
    logger.info("found port again " + port)
    # then connect to serial relay for log parsing
    dut.serial.connect_dut_log_port()

    logger.info("reset dut")
    ict_client.reset_dut()

    maybe_format(dut)
    dut.expect("Sol: Error-free boot!", timeout=30)
    logger.info("got boot message")

    # wait for ICT pageturner connect
    dut.expect("Connected to PageTurner", timeout=20)
    logger.info("got pageturner message")

    # wait 5 seconds
    time.sleep(5)

    clear_content_and_restart(ict_client)

    # at this point the dut will restart
    # wait again for startup messages

    dut.expect("Sol: Error-free boot!", timeout=30)
    logger.info("got boot message")

    # wait for ICT pageturner connect
    dut.expect("Connected to PageTurner", timeout=20)
    logger.info("got pageturner message")

    time.sleep(5.0)

    # content has been cleared and we have restarted,
    # so we should be on the content sync option now
    ict_client.tt_enter()

    # wait for wifi connection
    dut.expect("Connected, IP address", timeout=30)

    if full == True:
        dut.expect("books are in sync", timeout=180)
        dut.expect("articles are in sync", timeout=180)


@pytest.mark.usefixtures("ict_client")
@pytest.mark.usefixtures("power_client")
@pytest.mark.ict
@pytest.mark.power
@pytest.mark.fullsync
@pytest.mark.skipif(MFICTClient().get_ict_mode() != MODE_MAINFRAME_TESTER, reason="not mainframe tester")
class TestMainframeFullContentSync:
    def test_mainframe_full_content_sync(
        self, dut: OverrideDut, ict_client: MFICTClient, power_client: PowerTester, ssid, password
    ) -> None:
        logger.info("TestMainframeFullContentSync")
        generate_and_write_nvs(ict_client, dut, ssid, password)

        # Note: If the IdfDut tries to connect to the ICT serial port to flash it, it dumps a bunch of serial
        # garbage to the ICT that will cause it to fail the next command. This clears the input buffer by sending a newline.
        ict_client.clear_input()
        ict_client.set_power(False)
        time.sleep(0.5)

        # turn on battery power from PPK
        power_client.turn_on()

        # dut is in battery power mode, ready to perform full content sync
        perform_sync(dut, ict_client, ssid, password)


@pytest.mark.usefixtures("ict_client")
@pytest.mark.ict
@pytest.mark.fullsync
@pytest.mark.skipif(MFICTClient().get_ict_mode() != MODE_FINAL_TESTER, reason="not final tester")
class TestFinalFullContentSync:
    def test_final_full_content_sync(self, dut: OverrideDut, ict_client: MFICTClient, ssid, password) -> None:
        logger.info("TestFullContentSync")
        generate_and_write_nvs(ict_client, dut, ssid, password)

        # Note: If the IdfDut tries to connect to the ICT serial port to flash it, it dumps a bunch of serial
        # garbage to the ICT that will cause it to fail the next command. This clears the input buffer by sending a newline.
        ict_client.clear_input()
        logger.info("turn off usb power to dut")
        ict_client.set_power(False)

        # final tester now running on batteries, ready to perform full content sync
        perform_sync(dut, ict_client, ssid, password)
