import logging
import time

import serial
import os
import subprocess

from util import find_port_by_device_name

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

# test
curr_version = "27"

class MFICTClient:
    def __init__(self, baudrate=115200, timeout=1):
        # get first device with this name, other device is serial port relay
        self.port = find_port_by_device_name("SOL Mainframe ICT", 0)
        if self.port is None:
            # TODO: Retry?
            raise Exception("Failed to find ICT")
        logger.info("Found ICT on port: " + self.port)
        self.ser = serial.Serial(self.port, baudrate, timeout=timeout, rtscts=False, dsrdtr=False)

        # check if ICT update is required
        version = self.get_ict_version()
        logger.info("ICT version: " + version)

        if version != curr_version:
            self.flash_firmware(baudrate, timeout)

    def flash_firmware(self, baudrate, timeout):
        # reboot inot programming mode
        self.program()
        self.ser.close()
        self.ser = None

        logger.info("flashing firmware")
        time.sleep(5)

        # write nvs partition to dut
        res = subprocess.run(
            [
                "esptool.py",
                "--chip",
                "esp32s3",
                "-p",
                "/dev/ttyACM0",
                "-b",
                "460800",
                "--before=default_reset",
                "--after=hard_reset",
                "write_flash",
                "--flash_mode",
                "dio",
                "--flash_freq",
                "80m",
                "--flash_size",
                "2MB",
                "0x0",
                "mt.bin",
            ]
        )

        if res != 0:
            logger.info("failed to flash ict firmware")
            return

        logger.info("I am here")
        time.sleep(5)

        self.port = find_port_by_device_name("SOL Mainframe ICT", 0)
        if self.port is None:
            # TODO: Retry?
            raise Exception("Failed to find ICT")
        logger.info("Found ICT on port: " + self.port)
        self.ser = serial.Serial(self.port, baudrate, timeout=timeout, rtscts=False, dsrdtr=False)
        logger.info("flash successful")

    def flush_read_buffer(self):
        # Flush read buffer
        while self.ser.in_waiting > 0:
            logger.debug(f"ICT <-: {self.ser.readline()}")

    def write(self, command):
        out = (command + "\n").encode()
        logger.debug(f"ICT ->: {out}")
        self.ser.write(out)
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
        self.write("ON" if state else "OFF")
        self.flush_read_buffer()

    def reset_dut(self):
        self.write("RST")
        self.flush_read_buffer()

    def set_servo(self, servo, position):
        self.write(("LS" if servo == "left" else "RS") + " " + str(position))
        self.flush_read_buffer()

    def set_led_brightness(self, brightness):
        self.write("LED" + " " + str(brightness))
        self.flush_read_buffer()

    def get_voltage(self, voltage_type):
        self.ser.write((voltage_type + "\n").encode())
        time.sleep(0.1)
        voltage_line = self.ser.readline().decode().strip()
        if not voltage_line.startswith(voltage_type):
            raise Exception(f"Failed to get voltage '{voltage_type}', got response '{voltage_line}'")
        _, voltage_str = voltage_line.split(" ")
        return float(voltage_str)

    def get_magnet_status(self):
        self.ser.write("LMAG\n".encode())
        return bool(int(self.read().split(" ")[1]))

    def set_gpio0(self, level):
        self.write("IO0" + " " + str(level))
        self.flush_read_buffer()

    def clear_input(self):
        """Sometimes there can be garbage on the serial line due to pytest trying to use the ICT as the DUT. This routine gets us back to a clean state."""
        out = "\n".encode()
        self.ser.write(out)
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
        self.ser.write("BTMAC\n".encode())
        time.sleep(0.1)
        resp = self.read()
        btmac = resp.split(" ")[1]
        return btmac
    
    def get_ict_version(self):
        self.ser.write("VERSION\n".encode())
        time.sleep(0.1)
        resp = self.read()
        version = resp.split(" ")[1]
        return version
    
    def program(self):
        self.ser.write("PROGRAM\n".encode())
        #self.write("PROGRAM\n")
        #self.flush_read_buffer()
