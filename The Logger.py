import serial
import csv
import time

# Update 'COM3' to your actual ESP32 port
ser = serial.Serial('COM6', 115200)
file_name = "faulty_machine2.csv"  # Change to "faulty.csv" for the second run

print(f"Recording to {file_name}... Press Ctrl+C to stop.")

with open(file_name, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["ax", "ay", "az"])  # Header

    try:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                data = line.split(",")
                if len(data) == 3:
                    writer.writerow(data)
    except KeyboardInterrupt:
        print("Done recording.")
        ser.close()