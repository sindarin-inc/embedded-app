import argparse
import logging
import platform
import re
import time
from typing import Optional

import serial
from serial.tools import list_ports
from serial.tools.list_ports_common import ListPortInfo

logger = logging.getLogger("port_tool")


def find_windows_com_ports(vid: str, pid: str, serial_number: Optional[str] = None) -> list[str]:
    import win32com.client

    # Caution: GPT says `Win32_SerialPort` isn't reliable with USB devices.
    # But using `Win32_PnPEntity` requires parsing the COM port out of the
    # `Name`` which seems like it might break on Chinese language systems.
    # Seems to work for me.
    wmi = win32com.client.GetObject("winmgmts:")
    query = f"SELECT * FROM Win32_SerialPort"
    serial_ports = []

    for device in wmi.ExecQuery(query):
        logger.info(f"Found COM port: {device.DeviceID} {device.PNPDeviceID}")
        if device.PNPDeviceID and f"VID_{vid}&PID_{pid}" in device.PNPDeviceID:
            com_port = device.DeviceID
            # Extract MI_ number from DeviceID
            # Not exactly sure what this is but it seems to consistently enumerate in a
            # consistent order for the two USB devices, regardless of the COM port order
            mi_match = re.search(r"MI_([0-9A-F]+)", device.PNPDeviceID)
            mi = mi_match.group(1) if mi_match else "Unknown"

            # If we had a way to get the "Bus reported device description" here that'd work too
            # and be more consistent with other platforms. But we don't, and I guess it's hard?
            # See also: https://github.com/pyserial/pyserial/pull/338
            if serial_number is not None:
                ports = list_ports.grep(com_port)
                port = next(ports)
                if port.serial_number != serial_number:
                    logger.info(f"Skipping {com_port} ({serial_number} != {port.serial_number})")
                    continue

            serial_ports.append((com_port, mi))

    # Sort by the second field (MI number)
    serial_ports.sort(key=lambda x: x[1])

    return [port[0] for port in serial_ports]


def find_ports_matching(match_func: callable, sort: Optional[callable] = None) -> list[str]:
    ports: ListPortInfo = list_ports.comports()
    matching_ports: ListPortInfo = []
    for port in ports:
        if match_func(port):
            matching_ports.append(port)
    if sort is not None:
        matching_ports.sort(key=sort)

    return [port.device for port in matching_ports]


def find_ports_by_device_name(device_name: str) -> list[str]:
    return find_ports_matching(lambda port: device_name in port.description, lambda p: p.device)


def find_ports_by_hwid(hwid: str) -> list[str]:
    return find_ports_matching(lambda port: hwid in port.hwid, lambda p: p.device)


def find_ports_by_serial_number(serial_number: str) -> list[str]:
    # Sort by usb_info which is something like:
    # 'USB VID:PID=303A:4002 SER=SOLTEST' - for the tester control interface
    # 'USB VID:PID=303A:4002 SER=SOLTEST LOCATION=1-2.4.4:x.2' - for the tester logging interface
    # WARNING: This didn't provide a reliable ordering on Windows, so we're using the `find_windows_com_ports` method
    return find_ports_matching(
        lambda port: port.serial_number is not None and serial_number in port.serial_number,
        lambda p: p.usb_info(),
    )


def port_by_index(ports: list[str], index=0) -> Optional[str]:
    if len(ports) > index:
        return ports[index]

    return None


def wait_for_not_none(find_port: callable, timeout) -> Optional[str]:
    start_time = time.time()
    while True:
        port = find_port()
        if port is not None:
            return port
        if time.time() - start_time < timeout:
            time.sleep(0.1)
        else:
            break
    return None


def find_usb_jtag_ports() -> list[str]:
    if platform.system() == "Windows":
        return find_ports_by_hwid("USB VID:PID=303A:1001")
    else:
        return find_ports_by_device_name("USB JTAG/serial debug unit")


def find_usb_jtag_port() -> Optional[str]:
    return port_by_index(find_usb_jtag_ports())


def wait_for_usb_jtag_port(timeout=10) -> Optional[str]:
    port = wait_for_not_none(find_usb_jtag_port, timeout)
    return port


def find_control_port() -> Optional[str]:
    if platform.system() == "Windows":
        return port_by_index(find_windows_com_ports("303A", "4002", "SOLREADER"), 0)
    else:
        return port_by_index(find_ports_by_device_name("Sol Reader"), 0)


def wait_for_control_port(timeout=10) -> Optional[str]:
    port = wait_for_not_none(find_control_port, timeout)
    return port


def find_logging_port() -> Optional[str]:
    if platform.system() == "Windows":
        return port_by_index(find_windows_com_ports("303A", "4002", "SOLREADER"), 1)
    else:
        return port_by_index(find_ports_by_device_name("Sol Reader"), 1)


def wait_for_logging_port(timeout=10) -> Optional[str]:
    port = wait_for_not_none(find_logging_port, timeout)
    return port


def reset_usb_jtag_port(ser: serial.Serial, programming_mode: bool = False):
    """Useful to reset out of programming mode"""
    if ser.is_open:
        if programming_mode:
            # Programming mode
            ser.setDTR(True)
            ser.setRTS(True)
            ser.setDTR(False)
            ser.setRTS(False)
        else:
            # Just reboot
            ser.setDTR(True)
            ser.setRTS(False)
            ser.setDTR(False)
            ser.setRTS(True)


def main():
    logging.basicConfig(level=logging.WARN)
    parser = argparse.ArgumentParser(description="Tool for interacting with Sol Reader devices.")

    subparsers = parser.add_subparsers(dest="command", required=True, help="Sub-command to run")

    # Subparser for the find_port command
    find_parser = subparsers.add_parser("find_port", help="Find a specific type of port.")
    find_parser.add_argument(
        "port_type",
        choices=["logging", "programming", "jtag"],
        help="Specify the type of port to find.",
    )

    # Subparser for the reset_jtag command
    reset_parser = subparsers.add_parser("reset_jtag", help="Reset the device out of programming mode.")

    # Log level
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_const",
        dest="loglevel",
        const=logging.DEBUG,
        help="Increase verbosity to show DEBUG level logging.",
    )

    args = parser.parse_args()

    if args.loglevel:
        logging.getLogger().setLevel(args.loglevel)

    port = None
    if args.command == "reset_jtag":
        port = find_usb_jtag_port()
        if port:
            ser = serial.Serial(port, 115200, timeout=1)
            reset_usb_jtag_port(ser)
            ser.close()
        else:
            logging.error("Could not find USB JTAG port to reset.")
            exit(1)

    elif args.command == "find_port":
        if args.port_type == "logging":
            port = wait_for_control_port(3)
            if port:
                logging.debug(f"Found control port at {port}. Enabling logging.")
                ser = serial.Serial(port, 115200, timeout=1)
                ser.write(b"\nLOGGING_ON\n")
                ser.flush()
                ser.close()
                port = find_logging_port()
                if port:
                    print(port)
                else:
                    logging.error("Could not find logging port after enabling logging.")
                    exit(1)
            else:
                logging.error("Could not find control port to enable logging.")
                exit(1)

        elif args.port_type == "programming":
            port = find_usb_jtag_port()
            if not port:
                port = find_control_port()
                if port:
                    logging.debug(f"Found control port at {port}. Resetting into programming mode.")
                    ser = serial.Serial(port, 115200, timeout=1)
                    ser.write(b"\nPROGRAM\n")
                    ser.flush()
                    ser.close()
                    port = wait_for_usb_jtag_port(timeout=3)
                    if port:
                        print(port)
                    else:
                        logging.error("Could not find USB JTAG port after reset.")
                        exit(1)
                else:
                    logging.error("Could not find control port to reset into programming mode.")
                    exit(1)

        elif args.port_type == "jtag":
            port = wait_for_usb_jtag_port(timeout=3)
            if port:
                print(port)
            else:
                logging.error("Could not find USB JTAG port.")
                exit(1)


if __name__ == "__main__":
    main()
