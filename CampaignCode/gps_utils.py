# gps_utils.py
import serial
import pynmea2

def run_gps_during_test(port, baud=9600):
    """
    Continuously reads NMEA sentences from a GPS receiver and yields lat/lon
    when a valid fix is found.
    """
    with serial.Serial(port, baud, timeout=1) as ser:
        while True:
            line = ser.readline().decode('ascii', errors='replace').strip()
            if line.startswith('$GNRMC') or line.startswith('$GPRMC'):
                try:
                    msg = pynmea2.parse(line)
                    if msg.status == 'A':  # valid fix
                        yield msg.latitude, msg.longitude
                except pynmea2.ParseError:
                    continue
