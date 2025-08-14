#!/bin/bash

# Example script to run automated tests on the Sol Reader simulator

echo "Starting Sol Reader simulator with automation enabled..."

# Set environment variable to enable automation
export SOL_SIMULATOR_AUTOMATION=1

# Start simulator in background
make sim &
SIM_PID=$!

# Wait for simulator to start
echo "Waiting for simulator to start..."
sleep 3

# Check if automation socket is available
SOCKET_PATH="./sol_simulator_automation.sock"
MAX_WAIT=10
WAIT_COUNT=0

while [ ! -S "$SOCKET_PATH" ] && [ $WAIT_COUNT -lt $MAX_WAIT ]; do
    echo "Waiting for automation socket..."
    sleep 1
    WAIT_COUNT=$((WAIT_COUNT + 1))
done

if [ ! -S "$SOCKET_PATH" ]; then
    echo "Error: Automation socket not available after $MAX_WAIT seconds"
    kill $SIM_PID
    exit 1
fi

echo "Simulator automation ready!"

# Run test commands
echo "Taking initial screenshot..."
python3 scripts/control_simulator.py --screenshot

echo "Testing navigation..."
python3 scripts/control_simulator.py --key down
sleep 0.5
python3 scripts/control_simulator.py --key down
sleep 0.5
python3 scripts/control_simulator.py --key right
sleep 0.5

echo "Taking final screenshot..."
python3 scripts/control_simulator.py --screenshot

echo "Running test script..."
python3 scripts/control_simulator.py --script scripts/test_navigation.txt

echo "Test complete. Press Ctrl+C to exit simulator."
wait $SIM_PID