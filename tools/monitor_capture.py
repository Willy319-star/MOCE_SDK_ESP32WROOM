#!/usr/bin/env python3
import argparse
import signal
import sys
import time


running = True


def stop(_signum, _frame):
    global running
    running = False


def read_baud(project_dir):
    candidates = [
        "CONFIG_ESPTOOLPY_MONITOR_BAUD",
        "CONFIG_ESP_CONSOLE_UART_BAUDRATE",
    ]
    try:
        with open(f"{project_dir}/sdkconfig", "r", encoding="utf-8") as handle:
            lines = handle.readlines()
    except OSError:
        return 115200

    for key in candidates:
        prefix = f"{key}="
        for line in lines:
            if line.startswith(prefix):
                value = line[len(prefix):].strip().strip('"')
                try:
                    return int(value)
                except ValueError:
                    return 115200
    return 115200


def main():
    parser = argparse.ArgumentParser(description="Capture ESP serial monitor output without requiring a TTY.")
    parser.add_argument("project_dir")
    parser.add_argument("port")
    parser.add_argument("--baud", type=int, default=0)
    parser.add_argument("--duration", type=float, default=0)
    args = parser.parse_args()

    try:
        import serial
    except ImportError:
        print("ERROR: pyserial is not available in the active Python environment.", file=sys.stderr)
        return 2

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)

    baud = args.baud or read_baud(args.project_dir)
    deadline = time.monotonic() + args.duration if args.duration > 0 else None

    try:
        with serial.Serial(args.port, baudrate=baud, timeout=0.2) as ser:
            while running and (deadline is None or time.monotonic() < deadline):
                data = ser.readline()
                if not data:
                    continue
                text = data.decode("utf-8", errors="replace").rstrip("\r\n")
                print(text, flush=True)
    except serial.SerialException as error:
        print(f"ERROR: serial monitor failed: {error}", file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
