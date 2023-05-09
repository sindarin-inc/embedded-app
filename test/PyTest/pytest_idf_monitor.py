# SPDX-License-Identifier: CC0-1.0

import hashlib
import logging
from typing import Callable
import time
import subprocess  # To access the shell commands
import re  # To read through the output lines
import pexpect  # To run shell commands

import sol  # To access the sol.py file

import pytest
from pytest_embedded_idf.dut import IdfDut
from pytest_embedded_qemu.app import QemuApp
from pytest_embedded_qemu.dut import QemuDut


import pexpect.exceptions


@pytest.mark.supported_targets
@pytest.mark.preview_targets
@pytest.mark.generic

#  Defining the tests


def test_startup(dut: IdfDut) -> None:
    # sol.open_temple()
    # Sol: formatting littlefs..
    # Check for formatting
    # TODO: Check this conditionally
    try:
        dut.expect("Sol: Formatting LittleFS...", timeout=30)
    except pexpect.exceptions.TIMEOUT:
        pytest.fail("Failed: Did not format")

    try:
        dut.expect("Sol: Formatting LittleFS Complete.", timeout=300)
    except pexpect.exceptions.TIMEOUT:
        pytest.fail("Failed: Formatting failed")

    try:
        dut.expect("LTR329 ambient light sensor found", timeout=30)
    except pexpect.exceptions.TIMEOUT:
        pytest.fail("Failed: LTR329 ambient light sensor not found")

    try:
        dut.expect("MMC5603 magnetometer found", timeout=30)
    except pexpect.exceptions.TIMEOUT:
        pytest.fail("Failed: MMC5603 magnetometer not found")

    try:
        dut.expect("Sol: Error-free boot!", timeout=30)
    except pexpect.exceptions.TIMEOUT:
        pytest.fail("Failed: Sol: Error-free boot! not found")


# def test_power_on() -> None:
#     # Start with power on test
#     sol.open_temple()

#     max_out = 100
#     min_out = 50
#     test_duration = 15

#     # Run the power_on_test and get the average value
#     average_value = sol.power_on_test(max_out, min_out, test_duration)

#     # Check if the average value is within the expected range, otherwise fail the test
#     assert (
#         min_out <= average_value <= max_out
#     ), f"Failed Power On: The average value {average_value} is not within the range {min_out}mA to {max_out}mA"

#     # Deep sleep test


# def test_deep_sleep() -> None:
#     sol.close_temple()
#     max_out = 250
#     min_out = 50
#     test_duration = 15
#     # Run the power_on_test and get the average value
#     average_value = sol.deep_sleep_test(max_out, min_out, test_duration)

#     # Check if the average value is within the expected range, otherwise fail the test
#     assert (
#         min_out <= average_value <= max_out
#     ), f"Failed Deep Sleep: The average value {average_value} is not within the range {min_out}uA to {max_out}uA"

#     sol.detach_servo()


#     sol.close_temple()
#     dut.expect("Deep sleeping wakeup using magnetometer")
#     # dut.disconnect()
#     # Open question: Does pytest gracefully reconnect the serial?
#     sleep(1)
#     sol.expect_power((170, 250), 5)
#     sol.open_temple()
#     # dut.connect()
#     dut.expect("Error-free boot!")

# def test_remote_connected_power(dut: IdfDut) -> None:
#     dut.expect("Error-free boot!")

#     sol.connect_remote()
#     dut.expect("Page Turner connected")
#     sol.send_command(0xFFEE)
#     dut.expect("Page Turner received command: 0xFFEE")

#     sol.expect_power((60000, 70000), 5)


# # Check for Magnetometer sensor
# def test_mag_sensor(dut: IdfDut) -> None:
#     dut.expect("MMC5603 magnetometer found", timeout=90)


# # Check for error free boot
# def test_error_boot(dut: IdfDut) -> None:
#     dut.expect("Sol: Error-free boot!", timeout=90)
