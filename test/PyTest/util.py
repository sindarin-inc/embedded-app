import logging
import time

from pytest_embedded_idf.dut import IdfDut
from serial.tools import list_ports

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def find_ports_by_device_name(device_name):
    ports = list_ports.comports()
    devices = []
    for port in ports:
        if device_name in port.description:
            devices.append(port.device)

    return devices


def find_port_by_device_name(device_name, index=0):
    ports = list_ports.comports()
    devices = []
    for port in ports:
        if device_name in port.description:
            devices.append(port.device)

    if len(devices) > index:
        devices.sort()
        return devices[index]

    return None


def wait_for_port_by_device_name(device_name, index=0, timeout=5):
    start_time = time.time()
    while True:
        port = find_port_by_device_name(device_name, index)
        if port is not None:
            return port
        if time.time() - start_time < timeout:
            time.sleep(0.2)
        else:
            break
    raise Exception(f"Device '{device_name}' not found")


def maybe_format(dut: IdfDut) -> None:
    # This will return 0 if "Sol: Formatting LittleFS..." appears first,
    # or 1 if "Sol: Filesystem ready!" appears first
    formatting_string = "Sol: Formatting LittleFS..."
    match = dut.expect([formatting_string, "Sol: Filesystem ready!"], timeout=30)

    if match.group().decode() == formatting_string:
        logger.info("Formatting LittleFS (this takes ~4 minutes)")
        dut.expect("Sol: Formatting LittleFS Complete.", timeout=300)
        logger.info(f"Formatting complete")
