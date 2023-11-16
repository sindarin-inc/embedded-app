import logging
import os
import shutil
import subprocess
import time
import zipfile

import serial
from util import find_port_by_device_name, wait_for_port_by_device_name

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


class MFICTClient:
    def __init__(self, baudrate=115200, timeout=1):
        # get first device with this name, other device is serial port relay
        port = find_port_by_device_name("SOL Mainframe ICT", 0)
        if port is None:
            # TODO: Retry?
            raise Exception("Failed to find ICT")
        logger.info("Found ICT on port: " + port)
        self.ser = serial.Serial(port, baudrate, timeout=timeout, rtscts=False, dsrdtr=False)

        # check if ICT update is required
        version = self.get_ict_version()
        logger.info("ICT version: " + version)

        # find filename for firmware
        # this unzips the firmware files into a separate directory
        firmware_filename, script_filename, firmware_dir = self.get_firmware_filename()

        curr_version = self.get_firmware_version_string(firmware_filename)
        logger.info("ICT version for test: " + curr_version)

        if version != curr_version:
            self.flash_firmware(baudrate, timeout, firmware_filename, script_filename)

        # clean up, remove directory with unzipped firmware
        shutil.rmtree(firmware_dir)

    def get_firmware_filename(self):
        dir = "ict"
        results = []
        results += [each for each in os.listdir(dir) if each.endswith(".zip")]
        if len(results) != 1:
            logger.info("could not find ict firmware file")
            return ""

        # unzip file
        zip_fname = dir + "/" + results[0]
        with zipfile.ZipFile(zip_fname, "r") as zip_ref:
            zip_ref.extractall(dir)

        zip_dir = zip_fname.split(".")[0]
        firmware_files = os.listdir(zip_dir)
        results = []
        results += [each for each in firmware_files if each.endswith(".sh")]
        flash_script_file = zip_dir + "/" + results[0]

        results = []
        results += [each for each in firmware_files if each.endswith(".hex")]
        hex_file = zip_dir + "/" + results[0]

        return hex_file, flash_script_file, zip_dir

    def get_firmware_version_string(self, firmware_filename):
        f = open(firmware_filename, "rb")

        # version string is located at offset 0x30 within the app partition
        # thus, 0x10030
        f.seek(0x10030)

        # read characters into string until null character
        verstr = ""
        while True:
            char = f.read(1)
            if ord(char) == 0:
                break
            verstr += char.decode()
        return verstr

    def flash_firmware(self, baudrate, timeout, firmware_filename, script_filename):
        # power off dut, just in case it is on
        # prevents duplicate JTAG port from showing up
        self.set_power(False)

        # reboot into programming mode
        self.program()
        self.ser.close()
        self.ser = None

        # wait for JTAG port to appear
        port = wait_for_port_by_device_name("USB JTAG/serial debug unit", 0, timeout=10)
        if port is None:
            # TODO: Retry?
            raise Exception("Failed to find ICT JTAG")
        logger.info("Found ICT JTAG on port: " + port)

        # split arguments for esptool command to flash the firmware
        f = open(script_filename, "r")
        flash_script = f.read()
        flash_script_parts = flash_script.split(" ")
        parts = []
        for p in flash_script_parts:
            p = p.strip()
            if len(p) > 0:
                parts.append(p)

        # add the ICT port to the arguments
        # after first argument (esptool.py)
        parts.insert(1, "-p")
        parts.insert(2, port)

        # replace the firmware filename with the full path
        del parts[len(parts) - 1]
        parts.append(firmware_filename)
        flashcmd = " ".join(parts)

        logger.info("flashing firmware with command: " + flashcmd)

        # run esptool to flash ict firmware
        res = subprocess.run(parts)

        if res.returncode != 0:
            logger.info("failed to flash ict firmware")
            return

        logger.info("firmware flash complete, reconnecting to ICT")
        port = wait_for_port_by_device_name("SOL Mainframe ICT", 0)
        if port is None:
            # TODO: Retry?
            raise Exception("Failed to find ICT")
        logger.info("Found ICT on port: " + port)
        self.ser = serial.Serial(port, baudrate, timeout=timeout, rtscts=False, dsrdtr=False)
        logger.info("flash successful")

    def flush_read_buffer(self):
        # Flush read buffer
        while self.ser.in_waiting > 0:
            logger.debug(f"ICT <-: {self.ser.readline()}")

    def write(self, command):
        out = (command + "\n").encode()
        logger.debug(f"ICT ->: {out}")
        self.ser.write(out)

    def write_with_ack(self, command):
        self.write(command)
        # TODO: Would be nice to really wait instead of sleeping
        # Allow time for the message to be processed and an ACK to be sent
        time.sleep(0.1)
        ack = self.ser.readline()  # Wait for the acknowledgement
        logger.debug(f"ICT <-: {ack}")
        ack = ack.decode().strip()
        ack = ack.split(" ")[0]
        if ack != command.split(" ")[0]:
            raise Exception(f"Failed to execute command '{command}', got response '{ack}'")

    def read(self):
        if self.ser.isOpen():
            return self.ser.readline().decode().strip()
        return None

    def set_power(self, state):
        self.write_with_ack("ON" if state else "OFF")
        self.flush_read_buffer()

    def reset_dut(self):
        self.write_with_ack("RST")
        self.flush_read_buffer()

    def set_servo(self, servo, position):
        self.write_with_ack(("LS" if servo == "left" else "RS") + " " + str(position))
        self.flush_read_buffer()

    def set_led_brightness(self, brightness):
        self.write_with_ack("LED" + " " + str(brightness))
        self.flush_read_buffer()

    def get_voltage(self, voltage_type):
        self.write(voltage_type)
        time.sleep(0.1)
        voltage_line = self.ser.readline().decode().strip()
        if not voltage_line.startswith(voltage_type):
            raise Exception(f"Failed to get voltage '{voltage_type}', got response '{voltage_line}'")
        _, voltage_str = voltage_line.split(" ")
        return float(voltage_str)

    def get_magnet_status(self):
        self.write("LMAG")
        return bool(int(self.read().split(" ")[1]))

    def set_gpio0(self, level):
        self.write_with_ack("IO0" + " " + str(level))
        self.flush_read_buffer()

    def clear_input(self):
        """Sometimes there can be garbage on the serial line due to pytest trying to use the ICT as the DUT. This routine gets us back to a clean state."""
        self.write("")
        # TODO: Would be nice to really wait instead of sleeping
        # Allow time for the message to be processed and an ACK to be sent
        time.sleep(0.3)
        # Wait for the acknowledgement (Probably Invalid Message)
        self.flush_read_buffer()
        while self.ser.in_waiting > 0:
            ack = self.ser.readline()
            # We expect an Invalid command (since we sent a '\n')
            if ack != b"Invalid command \n":
                logger.debug(f"ICT <-: {self.ser.readline()}")

    def get_bt_mac(self):
        self.write("BTMAC")
        time.sleep(0.1)
        resp = self.read()
        btmac = resp.split(" ")[1]
        return btmac

    def get_wifi_ssid(self):
        self.write("SSID")
        time.sleep(0.1)
        resp = self.read()
        ssid = resp.split(" ")[1]
        return ssid

    def get_wifi_password(self):
        self.write("PASS")
        time.sleep(0.1)
        resp = self.read()
        password = resp.split(" ")[1]
        return password

    def tt_enter(self):
        self.write_with_ack("TTENT")
        self.flush_read_buffer()

    def get_ict_version(self):
        self.write("VERSION")
        time.sleep(0.1)
        resp = self.read()
        parts = resp.split(" ")
        if parts[0] == "Invalid":
            return "none"
        version = resp.split(" ")[1]
        return version

    def program(self):
        self.write("PROGRAM")
