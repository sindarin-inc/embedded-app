import csv
import logging
import statistics
import time

from ppk2_api.ppk2_api import PPK2_MP
from util import find_port_by_device_name

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)

file_index = 0


class PowerTester:
    def __init__(self):
        self.open()

    def open(self):
        port = find_port_by_device_name("PPK2")
        if port is None:
            raise Exception("Failed to find PPK2")
        logger.info("Found PPK2 on port: " + port)
        self.ppk = PPK2_MP(port, buffer_max_size_seconds=2)
        self.ppk.get_modifiers()
        self.ppk.use_source_meter()  # set source meter mode

    def close(self):
        self.ppk = None

    def turn_on(self):
        self.ppk.set_source_voltage(3800)  # set source voltage in mV
        self.ppk.toggle_DUT_power("ON")
        self.ppk.start_measuring()

    def turn_off(self):
        self.ppk.stop_measuring()
        self.ppk.toggle_DUT_power("OFF")

    def dump_samples_to_csv(self, filename, samples):
        with open(filename, mode="w", newline="") as file:
            writer = csv.writer(file)
            writer.writerow(["Time (ms)", "Current (uA)"])
            for index, sample in enumerate(samples):
                # Assumes 100k samples per second
                writer.writerow([index / 100, sample])

    def measure_current(self, duration: int):
        # Purge data already in the buffer
        while self.ppk.get_data() != b"":
            pass

        time.sleep(duration)

        start_time = time.time()
        read_data = b""
        while read_data == b"":
            # Sometimes this gets stuck and never returns data
            if time.time() - start_time > 5:
                raise Exception("Timeout while waiting for PPK2 data")
            read_data = self.ppk.get_data()
        samples = []
        if read_data != b"":
            samples = self.ppk.get_samples(read_data)
        else:
            raise Exception("No data received from PPK2")

        # If samples is a tuple, just take the first part of it
        # This is to handle a breaking change from https://github.com/IRNAS/ppk2-api-python/pull/33
        if isinstance(samples, tuple):
            samples = samples[0]

        # Calculate average current
        average_current = statistics.mean(samples)
        logger.info(f"Average of {len(samples)} samples is: {average_current:0.1f}uA")
        return average_current, samples

    def expect_current(self, range: tuple[int, int], time: int):
        # Sometimes the PPK2 likes to return negative values. Retry until we get a positive value.
        # NOTE(john): Since this happens very sporadically I haven't tested this much. It might be that we actually need to close and re-open the serial port (and turn back on power, etc)
        average_current = -1
        attempts = 0
        while average_current < 0 and attempts < 3:
            average_current, samples = self.measure_current(time)
            if average_current < 0:
                logger.warn(f"Got negative average current: {average_current:0.1f}uA, retrying...")
            attempts += 1

        global file_index
        self.dump_samples_to_csv(f"current_samples_{file_index}-{range[0]}-{range[1]}.csv", samples)
        file_index += 1

        assert (
            range[0] <= average_current <= range[1]
        ), f"Average current out of range: {average_current:0.1f} not in {range}"
