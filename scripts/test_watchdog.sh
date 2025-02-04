#!/bin/bash

# Check for required parameters
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <test_command> <timeout_in_seconds>"
    exit 1
fi

TEST_CMD=$1  # The test command to run
TIMEOUT=$2   # Timeout in seconds

# Extract the binary path (first component of TEST_CMD)
BINARY_PATH=$(echo $TEST_CMD | awk '{print $1}')

# Needed to enable core dumps
ulimit -c unlimited

# Set up the core dump directory and pattern
sudo mkdir -p /cores
sudo chmod 777 /cores
sudo bash -c 'echo "/cores/%e.%p" > /proc/sys/kernel/core_pattern'

# Run the test command in the background
$TEST_CMD &
TEST_PID=$!

# Monitor the process for a timeout
SECONDS_ELAPSED=0
while kill -0 $TEST_PID 2> /dev/null; do
    sleep 1
    SECONDS_ELAPSED=$((SECONDS_ELAPSED + 1))

    if [ "$SECONDS_ELAPSED" -ge "$TIMEOUT" ]; then
        echo "Test suite timed out. Sending SIGSEGV to generate core dump..."
        kill -SIGSEGV $TEST_PID
        wait $TEST_PID  # Wait for the process to terminate and generate core dump
        break
    fi
done

# Wait for the test to finish and check for a core dump if it crashed
wait $TEST_PID
EXIT_CODE=$?

# If there’s a core dump, analyze it
if [ $EXIT_CODE -ne 0 ]; then
    ls /cores
    COREFILE=$(ls /cores/*.$TEST_PID 2>/dev/null | head -n 1)
    if [ -f "$COREFILE" ]; then
        echo "Analyzing core dump with gdb..."
        gdb -q -ex "file $BINARY_PATH" -ex "core-file $COREFILE" -ex "thread apply all bt" -ex "quit"
    else
        echo "Test suite crashed, but no core dump was found."
    fi
fi

exit $EXIT_CODE
