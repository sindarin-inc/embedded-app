#!/usr/bin/env python3
"""
Control the Sol Reader simulator programmatically.

Usage:
    python control_simulator.py --key up
    python control_simulator.py --sequence "uuddlrlr"  # up up down down left right left right
    python control_simulator.py --screenshot
    python control_simulator.py --wait 1000
    python control_simulator.py --script test_navigation.txt
"""

import socket
import sys
import argparse
import time
import os

SOCKET_PATH = "/tmp/sol_simulator_automation.sock"

def send_command(command):
    """Send a command to the simulator and get response."""
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(SOCKET_PATH)
        sock.send(command.encode())
        response = sock.recv(1024).decode()
        sock.close()
        return response
    except Exception as e:
        print(f"Error: {e}")
        return None

def send_key(key):
    """Send a single key press."""
    key_map = {
        'up': 'u',
        'down': 'd', 
        'left': 'l',
        'right': 'r',
        'enter': '\n',
        'escape': '\e',
        'back': '\e'
    }
    
    if key.lower() in key_map:
        key = key_map[key.lower()]
    elif len(key) > 1:
        print(f"Unknown key: {key}")
        return False
    
    response = send_command(f"KEY:{key}")
    return response and "OK" in response

def send_sequence(sequence, delay_ms=100):
    """Send a sequence of keys with delays."""
    response = send_command(f"KEYSEQ:{sequence}")
    return response and "OK" in response

def take_screenshot():
    """Take a screenshot of the simulator."""
    response = send_command("SCREENSHOT")
    if response and "OK" in response:
        print("Screenshot saved to /tmp/simulator_screenshot.png")
        return True
    return False

def wait(ms):
    """Wait for specified milliseconds."""
    response = send_command(f"WAIT:{ms}")
    return response and "OK" in response

def run_script(script_file):
    """Run a script file with commands."""
    with open(script_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            
            parts = line.split()
            if not parts:
                continue
            
            cmd = parts[0].lower()
            
            if cmd == 'key' and len(parts) > 1:
                send_key(parts[1])
            elif cmd == 'sequence' and len(parts) > 1:
                send_sequence(parts[1])
            elif cmd == 'screenshot':
                take_screenshot()
            elif cmd == 'wait' and len(parts) > 1:
                wait(int(parts[1]))
            elif cmd == 'sleep' and len(parts) > 1:
                time.sleep(float(parts[1]) / 1000)
            else:
                print(f"Unknown command: {line}")

def main():
    parser = argparse.ArgumentParser(description='Control Sol Reader Simulator')
    parser.add_argument('--key', help='Send a single key press')
    parser.add_argument('--sequence', help='Send a sequence of keys')
    parser.add_argument('--screenshot', action='store_true', help='Take a screenshot')
    parser.add_argument('--wait', type=int, help='Wait for milliseconds')
    parser.add_argument('--script', help='Run a script file')
    
    args = parser.parse_args()
    
    if not os.path.exists(SOCKET_PATH):
        print(f"Error: Simulator not running or automation not enabled.")
        print(f"Socket {SOCKET_PATH} does not exist.")
        sys.exit(1)
    
    if args.key:
        if send_key(args.key):
            print(f"Sent key: {args.key}")
    elif args.sequence:
        if send_sequence(args.sequence):
            print(f"Sent sequence: {args.sequence}")
    elif args.screenshot:
        take_screenshot()
    elif args.wait:
        if wait(args.wait):
            print(f"Waited {args.wait}ms")
    elif args.script:
        run_script(args.script)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()