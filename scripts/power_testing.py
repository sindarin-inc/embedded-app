import subprocess # To access the shell commands
import re # To read through the output lines
import time
import pexpect # To run shell commands

# # Creating a Log File
log_file = open('power_log.txt', 'wb')

# # Using Pexcpect to run the commands
shell_session = pexpect.spawn('/bin/bash', ['-i'], logfile=log_file)


import time
import serial
import struct
import logging
import os
import queue
import multiprocessing
import matplotlib.pyplot as plt
import numpy as np
import gc

class PPK2_Command():
    """Serial command opcodes"""
    NO_OP = 0x00
    TRIGGER_SET = 0x01
    AVG_NUM_SET = 0x02  # no-firmware
    TRIGGER_WINDOW_SET = 0x03
    TRIGGER_INTERVAL_SET = 0x04
    TRIGGER_SINGLE_SET = 0x05
    AVERAGE_START = 0x06
    AVERAGE_STOP = 0x07
    RANGE_SET = 0x08
    LCD_SET = 0x09
    TRIGGER_STOP = 0x0a
    DEVICE_RUNNING_SET = 0x0c
    REGULATOR_SET = 0x0d
    SWITCH_POINT_DOWN = 0x0e
    SWITCH_POINT_UP = 0x0f
    TRIGGER_EXT_TOGGLE = 0x11
    SET_POWER_MODE = 0x11
    RES_USER_SET = 0x12
    SPIKE_FILTERING_ON = 0x15
    SPIKE_FILTERING_OFF = 0x16
    GET_META_DATA = 0x19
    RESET = 0x20
    SET_USER_GAINS = 0x25


# Plotting tools
def plot_time_vs_samples(time, samples):
    fig, ax = plt.subplots()
    ax.plot(time, samples)
    ax.set(xlabel='Time (s)', ylabel='Current(uA)',
        title='Time vs Current Plot')
    ax.grid()
    plt.show()

    
class PPK2_Modes():
    """PPK2 measurement modes"""
    AMPERE_MODE = "AMPERE_MODE"
    SOURCE_MODE = "SOURCE_MODE"


class PPK2_API():
    def __init__(self, port):

        self.ser = None
        self.ser = serial.Serial(port)
        self.ser.baudrate = 9600

        self.modifiers = {
            "Calibrated": None,
            "R": {"0": 1031.64, "1": 101.65, "2": 10.15, "3": 0.94, "4": 0.043},
            "GS": {"0": 1, "1": 1, "2": 1, "3": 1, "4": 1},
            "GI": {"0": 1, "1": 1, "2": 1, "3": 1, "4": 1},
            "O": {"0": 0, "1": 0, "2": 0, "3": 0, "4": 0},
            "S": {"0": 0, "1": 0, "2": 0, "3": 0, "4": 0},
            "I": {"0": 0, "1": 0, "2": 0, "3": 0, "4": 0},
            "UG": {"0": 1, "1": 1, "2": 1, "3": 1, "4": 1},
            "HW": None,
            "IA": None
        }

        self.vdd_low = 800
        self.vdd_high = 5000

        self.current_vdd = None

        self.adc_mult = 1.8 / 163840

        self.MEAS_ADC = self._generate_mask(14, 0)
        self.MEAS_RANGE = self._generate_mask(3, 14)
        self.MEAS_LOGIC = self._generate_mask(8, 24)

        self.mode = None

        self.rolling_avg = None
        self.rolling_avg4 = None
        self.prev_range = None
        self.consecutive_range_samples = 0

        self.spike_filter_alpha = 0.18
        self.spike_filter_alpha5 = 0.06
        self.spike_filter_samples = 3
        self.after_spike = 0

        # adc measurement buffer remainder and len of remainder
        self.remainder = {"sequence": b'', "len": 0}

    def __del__(self):
        """Destructor"""
        try:
            if self.ser:
                self.ser.close()
        except Exception as e:
            logging.error(f"An error occured while closing ppk2_api: {e}")

    def _pack_struct(self, cmd_tuple):
        """Returns packed struct"""
        return struct.pack("B" * len(cmd_tuple), *cmd_tuple)

    def _write_serial(self, cmd_tuple):
        """Writes cmd bytes to serial"""
        try:
            cmd_packed = self._pack_struct(cmd_tuple)
            self.ser.write(cmd_packed)
        except Exception as e:
            logging.error(f"An error occured when writing to serial port: {e}")

    def _twos_comp(self, val):
        """Compute the 2's complement of int32 value"""
        if (val & (1 << (32 - 1))) != 0:
            val = val - (1 << 32)  # compute negative value
        return val

    def _convert_source_voltage(self, mV):
        """Convert input voltage to device command"""
        # minimal possible mV is 800
        if mV < self.vdd_low:
            mV = self.vdd_low

        # maximal possible mV is 5000
        if mV > self.vdd_high:
            mV = self.vdd_high

        offset = 32
        # get difference to baseline (the baseline is 800mV but the initial offset is 32)
        diff_to_baseline = mV - self.vdd_low + offset
        base_b_1 = 3
        base_b_2 = 0  # is actually 32 - compensated with above offset

        # get the number of times we have to increase the first byte of the command
        ratio = int(diff_to_baseline / 256)
        remainder = diff_to_baseline % 256  # get the remainder for byte 2

        set_b_1 = base_b_1 + ratio
        set_b_2 = base_b_2 + remainder

        return set_b_1, set_b_2

    def _read_metadata(self):
        """Read metadata"""
        # try to get metadata from device
        for _ in range(0, 5):
            # it appears the second reading is the metadata
            read = self.ser.read(self.ser.in_waiting)
            time.sleep(0.1)

            # TODO add a read_until serial read function with a timeout
            if read != b'' and "END" in read.decode("latin-1"):
                return read.decode("utf-8")

    def _parse_metadata(self, metadata):
        """Parse metadata and store it to modifiers"""
        # TODO handle more robustly
        try:
            data_split = [row.split(": ") for row in metadata.split("\n")]

            for key in self.modifiers.keys():
                for data_pair in data_split:
                    if key == data_pair[0]:
                        self.modifiers[key] = data_pair[1]
                    for ind in range(0, 5):
                        if key+str(ind) == data_pair[0]:
                            if "R" in data_pair[0]:
                                # problem on some PPK2s with wrong calibration values - this doesn't fix it
                                if float(data_pair[1]) != 0:
                                    self.modifiers[key][str(ind)] = float(
                                        data_pair[1])
                            else:
                                self.modifiers[key][str(ind)] = float(
                                    data_pair[1])
            return True
        except Exception as e:
            # if exception triggers serial port is probably not correct
            return None

    def _generate_mask(self, bits, pos):
        pos = pos
        mask = ((2**bits-1) << pos)
        mask = self._twos_comp(mask)
        return {"mask": mask, "pos": pos}

    def _get_masked_value(self, value, meas):
        masked_value = (value & meas["mask"]) >> meas["pos"]
        if meas["pos"] == 24:
            if masked_value == 255:
                masked_value = -1
        return masked_value

    def _handle_raw_data(self, adc_value):
        """Convert raw value to analog value"""
        try:
            current_measurement_range = min(self._get_masked_value(
                adc_value, self.MEAS_RANGE), 4)  # 5 is the number of parameters
            adc_result = self._get_masked_value(adc_value, self.MEAS_ADC) * 4
            bits = self._get_masked_value(adc_value, self.MEAS_LOGIC)
            analog_value = self.get_adc_result(
                current_measurement_range, adc_result) * 10**6
            return analog_value
        except Exception as e:
            print("Measurement outside of range!")
            return None

    @staticmethod
    def list_devices():
        import serial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        if os.name == 'nt':
            devices = [port.device for port in ports if port.description.startswith("nRF Connect USB CDC ACM")]
        else:
            devices = [port.device for port in ports if port.product == 'PPK2']
        return devices

    def get_data(self):
        """Return readings of one sampling period"""
        sampling_data = self.ser.read(self.ser.in_waiting)
        return sampling_data

    def get_modifiers(self):
        """Gets and sets modifiers from device memory"""
        self._write_serial((PPK2_Command.GET_META_DATA, ))
        metadata = self._read_metadata()
        ret = self._parse_metadata(metadata)
        return ret

    def start_measuring(self):
        """Start continuous measurement"""
        if not self.current_vdd:
            if self.mode == PPK2_Modes.SOURCE_MODE:
                raise Exception("Output voltage not set!")
            if self.mode == PPK2_Modes.AMPERE_MODE:
                raise Exception("Input voltage not set!")

        self._write_serial((PPK2_Command.AVERAGE_START, ))

    def stop_measuring(self):
        """Stop continuous measurement"""
        self._write_serial((PPK2_Command.AVERAGE_STOP, ))

    def set_source_voltage(self, mV):
        """Inits device - based on observation only REGULATOR_SET is the command. 
        The other two values correspond to the voltage level.

        800mV is the lowest setting - [3,32] - the values then increase linearly
        """
        b_1, b_2 = self._convert_source_voltage(mV)
        self._write_serial((PPK2_Command.REGULATOR_SET, b_1, b_2))
        self.current_vdd = mV

    def toggle_DUT_power(self, state):
        """Toggle DUT power based on parameter"""
        if state == "ON":
            self._write_serial(
                (PPK2_Command.DEVICE_RUNNING_SET, PPK2_Command.TRIGGER_SET))  # 12,1

        if state == "OFF":
            self._write_serial(
                (PPK2_Command.DEVICE_RUNNING_SET, PPK2_Command.NO_OP))  # 12,0

    def use_ampere_meter(self):
        """Configure device to use ampere meter"""
        self.mode = PPK2_Modes.AMPERE_MODE
        self._write_serial((PPK2_Command.SET_POWER_MODE,
                            PPK2_Command.TRIGGER_SET))  # 17,1

    def use_source_meter(self):
        """Configure device to use source meter"""
        self.mode = PPK2_Modes.SOURCE_MODE
        self._write_serial((PPK2_Command.SET_POWER_MODE,
                            PPK2_Command.AVG_NUM_SET))  # 17,2

    def get_adc_result(self, current_range, adc_value):
        """Get result of adc conversion"""
        current_range = str(current_range)
        result_without_gain = (adc_value - self.modifiers["O"][current_range]) * (
            self.adc_mult / self.modifiers["R"][current_range])
        adc = self.modifiers["UG"][current_range] * (result_without_gain * (self.modifiers["GS"][current_range] * result_without_gain + self.modifiers["GI"][current_range]) + (
            self.modifiers["S"][current_range] * (self.current_vdd / 1000) + self.modifiers["I"][current_range]))

        prev_rolling_avg = self.rolling_avg
        prev_rolling_avg4 = self.rolling_avg4

        # spike filtering / rolling average
        if self.rolling_avg is None:
            self.rolling_avg = adc
        else:
            self.rolling_avg = self.spike_filter_alpha * adc + (1 - self.spike_filter_alpha) * self.rolling_avg
        
        if self.rolling_avg4 is None:
            self.rolling_avg4 = adc
        else:
            self.rolling_avg4 = self.spike_filter_alpha5 * adc + (1 - self.spike_filter_alpha5) * self.rolling_avg4

        if self.prev_range is None:
            self.prev_range = current_range

        if self.prev_range != current_range or self.after_spike > 0:
            if self.prev_range != current_range:
                self.consecutive_range_samples = 0
                self.after_spike = self.spike_filter_samples
            else:
                self.consecutive_range_samples += 1

            if current_range == "4":
                if self.consecutive_range_samples < 2:
                    self.rolling_avg = prev_rolling_avg
                    self.rolling_avg4 = prev_rolling_avg4
                adc = self.rolling_avg4
            else:
                adc = self.rolling_avg
            
            self.after_spike -= 1

        self.prev_range = current_range
        return adc

    def _digital_to_analog(self, adc_value):
        """Convert discrete value to analog value"""
        return int.from_bytes(adc_value, byteorder="little", signed=False)  # convert reading to analog value

    def get_samples(self, buf):
        """
        Returns list of samples read in one sampling period.
        The number of sampled values depends on the delay between serial reads.
        Manipulation of samples is left to the user.
        See example for more info.
        """

        sample_size = 4  # one analog value is 4 bytes in size
        offset = self.remainder["len"]
        samples = []

        first_reading = (
            self.remainder["sequence"] + buf[0:sample_size-offset])[:4]
        adc_val = self._digital_to_analog(first_reading)
        measurement = self._handle_raw_data(adc_val)
        if measurement is not None:
            samples.append(measurement)

        offset = sample_size - offset

        while offset <= len(buf) - sample_size:
            next_val = buf[offset:offset + sample_size]
            offset += sample_size
            adc_val = self._digital_to_analog(next_val)
            measurement = self._handle_raw_data(adc_val)
            if measurement is not None:
                samples.append(measurement)

        self.remainder["sequence"] = buf[offset:len(buf)]
        self.remainder["len"] = len(buf)-offset

        return samples  # return list of samples, handle those lists in PPK2 API wrapper


class PPK_Fetch(multiprocessing.Process):
    '''
    Background process for polling the data in multiprocessing variant
    '''
    def __init__(self, ppk2, quit_evt, buffer_len_s=10, buffer_chunk_s=0.5):
        super().__init__()
        self._ppk2 = ppk2
        self._quit = quit_evt

        self.print_stats = False
        self._stats = (None, None)
        self._last_timestamp = 0

        self._buffer_max_len = int(buffer_len_s * 100000 * 4)    # 100k 4-byte samples per second
        self._buffer_chunk = int(buffer_chunk_s * 100000 * 4)    # put in the queue in chunks of 0.5s

        # round buffers to a whole sample
        if self._buffer_max_len % 4 != 0:
            self._buffer_max_len = (self._buffer_max_len // 4) * 4
        if self._buffer_chunk % 4 != 0:
            self._buffer_chunk = (self._buffer_chunk // 4) * 4

        self._buffer_q = multiprocessing.Queue()

    def run(self):
        s = 0
        t = time.time()
        local_buffer = b''
        while not self._quit.is_set():
            d = PPK2_API.get_data(self._ppk2)
            tm_now = time.time()
            local_buffer += d
            while len(local_buffer) >= self._buffer_chunk:
                # FIXME: check if lock might be needed when discarding old data
                self._buffer_q.put(local_buffer[:self._buffer_chunk])
                while self._buffer_q.qsize()>self._buffer_max_len/self._buffer_chunk:
                    self._buffer_q.get()
                local_buffer = local_buffer[self._buffer_chunk:]
                self._last_timestamp = tm_now
                #print(len(d), len(local_buffer), self._buffer_q.qsize())

            # calculate stats
            s += len(d)
            dt = tm_now - t
            if dt >= 0.1:
                if self.print_stats:
                    print(f"Samples: {s}, delta time: {dt}")
                self._stats = (s, dt)
                s = 0
                t = tm_now

            time.sleep(0.0001)

        # process would hang on join() if there's data in the buffer after the measurement is done
        while True:
            try:
                self._buffer_q.get(block=False)
            except queue.Empty:
                break

    def get_data(self):
        ret = b''
        count = 0
        while True:
            try:
                ret += self._buffer_q.get(timeout=0.01) # get_nowait sometimes skips a chunk for some reason
                count += 1
            except queue.Empty:
                break
        return ret


class PPK2_MP(PPK2_API):
    '''
    Multiprocessing variant of the object. The interface is the same as for the regular one except it spawns
    a background process on start_measuring()
    '''
    def __init__(self, port, buffer_seconds=10):
        '''
        port - port where PPK2 is connected
        buffer_seconds - how many seconds of data to keep in the buffer
        '''
        super().__init__(port)
        self._fetcher = None
        self._quit_evt = multiprocessing.Event()
        self._buffer_seconds = buffer_seconds

    def __del__(self):
        """Destructor"""
        PPK2_API.stop_measuring(self)
        self._quit_evt.clear()
        self._quit_evt = None
        del self._quit_evt
        if self._fetcher is not None:
            self._fetcher.join()
        self._fetcher = None
        del self._fetcher

    def start_measuring(self):
        # discard the data in the buffer
        self.stop_measuring()
        while self.get_data()!=b'':
            pass

        PPK2_API.start_measuring(self)
        self._quit_evt.clear()
        if self._fetcher is not None:
            return
        
        self._fetcher = PPK_Fetch(self, self._quit_evt, self._buffer_seconds)
        self._fetcher.start()

    def stop_measuring(self):
        PPK2_API.stop_measuring(self)
        self.get_data() # flush the serial buffer (to prevent unicode error on next command)
        self._quit_evt.set()
        if self._fetcher is not None:
            self._fetcher.join() # join() will block if the queue isn't empty
            self._fetcher = None

    def get_data(self):
        try:
            return self._fetcher.get_data()
        except (TypeError, AttributeError):
            return b''

power_check_conditions =[
    'Power On Consumption',
    'Deep Sleep Consumption',
]


# Set up some servo stuff
def send_command(arduino, command):
    arduino.write((command + '\n').encode('utf-8'))
    time.sleep(0.1)

power_on_pos = 'p10'
deep_sleep_pos = 'p100'
# Replace 'COM3' with the appropriate serial port for your Arduino
arduino_port = "/dev/cu.usbserial-DA013Z0F"
baud_rate = 9600

# Open the serial connection
arduino = serial.Serial(arduino_port, baud_rate, timeout=2)

# Ports for PPK2
ppk2_test_port= "/dev/tty.usbmodemF03C096037012" # Change this to the port your PPK2 is connected to

# read measured values in a for loop like this:
no_of_cycles = 3 #input("Enter the number of cyles: ") # No of cycles you want to run the test for
no_of_cycles = int(no_of_cycles) # Converting it into an integer
k = 400    #Time calculation constant
power_on_run_time = 12 # Time in sec to run the glasses in power on mode
deep_sleep_run_time = 15 # Time in sec to run the glasses in deep sleep mode
power_on_run_time = power_on_run_time*k   #Runtime calculation for next step (Can change this to fit any device)
deep_sleep_run_time = deep_sleep_run_time*k   #Runtime calculation for next step (Can change this to fit any device)
samples = []
power_on_pass_condition = 100; # Max current consumption during and post boot (in mA)
power_on_fail_condition = 10; # Max current consumption during and post boot (in mA)
deep_sleep_pass_condition = 200; # Max current consumption during and post boot (in uA)
data_ignore_const = 3; # Ignores a third of the samples at the beginning of the test

## POWER ON TESTING CONDITIONS
power_on_data = np.zeros(int(round(power_on_run_time)))

for test in range(no_of_cycles):
    # Using one port to move the servo first
    with serial.Serial(arduino_port, baud_rate, timeout=1) as servo:
        time.sleep(3)  # Allow time for the connection to be established
    # Move the servo arm to mimic power on state
        send_command(arduino, str(power_on_pos))
    # Close the servo port

    # Use a different port to start the PPK2
    ppk2_test = PPK2_API(ppk2_test_port)  # serial port will be different for you
    ppk2_test.get_modifiers()
    ppk2_test.use_source_meter()  # set source meter mode
    test_voltage = 4000  #int(input("Enter test voltage(in mV): "))
    ppk2_test.set_source_voltage(test_voltage)  # set source voltage in mV
    ppk2_test.toggle_DUT_power("ON")
    ppk2_test.start_measuring()  # start measuring
    # tic = time.perf_counter() # Timer Start
    print("Test No: " + str(test+1))
    for i in range(0, int(round(power_on_run_time))):
        read_data = ppk2_test.get_data()
        if read_data != b'':
            samples = ppk2_test.get_samples(read_data)
            power_on_data[i] = (sum(samples))/(len(samples))
            # print(f"Average of {len(samples)} samples is: {sum(samples)/len(samples)}uA")
        time.sleep(0.001)  # lower time between sampling -> less samples read in one sampling period

    # toc = time.perf_counter()  # Timer Off
    #print("Power on test ran for "+ str((toc-tic))+" seconds.")

    # print("Power On Consumption: " + str(np.mean(power_on_data[int(round((run_time)/6)):])) + "uA")
    data_len_p = len(power_on_data)
    start_index_p = int(round(data_len_p / 2))
    power_on_consum = np.mean(power_on_data[start_index_p:])/1000 #1000 is the conversion factor for the output in mA (experimentally determined)
    # power_on_consum = np.mean(power_on_data[int(round(power_on_run_time/data_ignore_const)):])/1000 #1000 is the conversion factor for the output in mA (experimentally determined)

    if power_on_consum < power_on_pass_condition and power_on_consum > power_on_fail_condition:
        print("Average Power On Consumption: "+ str(power_on_consum) + "mA")
        print("Power On Consumption Test: PASS")
    else:    
        print("Power On Consumption: FAILED")
        print("Average Power On Consumption: "+ str(power_on_consum) + "mA")

    ppk2_test.stop_measuring()
    del ppk2_test
    gc.collect()

    # Close the servo port

    # Disconnecting the PPK2 port


    ## DEEP SLEEP TESTING CONDITIONS
       # Using one port to move the servo first
    with serial.Serial(arduino_port, baud_rate, timeout=1) as servo:
        time.sleep(3)  # Allow time for the connection to be established
    # Move the servo arm to mimic power on state
        send_command(arduino, str(deep_sleep_pos))
        time.sleep(2) # Wait 2s before starting the test
    # Close the servo portss

    deep_sleep_data = np.zeros(int(round(deep_sleep_run_time))) # Predefining the array for the deep sleep data

    # time.sleep(10) # Wait 10s before starting the test
    ppk2_test = PPK2_API(ppk2_test_port)  # serial port will be different for you
    ppk2_test.get_modifiers()
    ppk2_test.use_source_meter()  # set source meter mode
    test_voltage = 4000  #int(input("Enter test voltage(in mV): "))
    ppk2_test.set_source_voltage(test_voltage)  # set source voltage in mV
    ppk2_test.toggle_DUT_power("ON")
    ppk2_test.start_measuring()  # start measuring
    
    # tic = time.perf_counter() # Timer Start

    for i in range(0,int(round(deep_sleep_run_time))):
        read_data = ppk2_test.get_data()
        if read_data != b'':
            samples = ppk2_test.get_samples(read_data)
            deep_sleep_data[i] = (sum(samples))/(len(samples))
            # print(f"Average of {len(samples)} samples is: {sum(samples)/len(samples)}uA")
        time.sleep(0.001)  # lower time between sampling -> less samples read in one sampling period

    # toc = time.perf_counter()
    #print("Deep Sleep test ran for "+ str((toc-tic))+" seconds.")

    # print("Power On Consumption: " + str(np.mean(power_on_data[int(round((run_time)/6)):])) + "uA")
    data_len = len(deep_sleep_data)
    start_index = int(round(data_len / 2))
    deep_sleep_consum = np.mean(deep_sleep_data[start_index:])
    # deep_sleep_consum = np.mean(deep_sleep_data[int(round(deep_sleep_run_time/data_ignore_const)):]) # Finding the average deep sleep consumption

    # print("Average Deep Sleep Consumption: "+ str(deep_sleep_consum) + "uA")
    if deep_sleep_consum < deep_sleep_pass_condition:
        print("Average Deep Sleep Consumption: "+ str(deep_sleep_consum) + "uA")
        print("Deep Sleep Test: PASS")
    else:    
        print("Deep Sleep Test: FAILED")
        print("Average Deep Sleep Consumption: "+ str(deep_sleep_consum) + "uA")

    ppk2_test.stop_measuring()
    del ppk2_test
    gc.collect()
with serial.Serial(arduino_port, baud_rate, timeout=1) as servo:
        time.sleep(2)  # Allow time for the connection to be established
    # Move the servo arm to mimic power on state
        send_command(arduino, str(deep_sleep_pos))

# Close the log file
log_file.close()
# Close the shell session
shell_session.close()