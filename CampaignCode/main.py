import json
import subprocess
import time
from datetime import datetime
from zoneinfo import ZoneInfo
from ifstat_utils import run_ifstat_during_test  
from gps_utils import run_gps_during_test

INTERFACE1 = "wlp87s0f0"
INTERFACE2 = "wlx40a5ef538611"

PORT = "/dev/ttyACM0"

AWS_SERVER = "47.129.143.46"
DURATION = 10
PAUSE = 2
OUTPUT_FILE = "iperf3_results.csv"

with open(OUTPUT_FILE, "w") as f:
    f.write("timestamp,iperf3_Mbps,RX_Mbps,TX_Mbps\n")

    while True:
        timestamp = datetime.now(ZoneInfo("Asia/Singapore")).strftime("%H:%M:%S")

        # Run iperf3 with mptcp
        iperf_cmd = ["mptcpize", "run", "iperf3", "-c", AWS_SERVER, "-t", str(DURATION), "-J"]
        iperf_proc = subprocess.Popen(iperf_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        # Run ifstat for same duration in parallel
        rxSpeed1, txSpeed1 = run_ifstat_during_test(INTERFACE1, DURATION)
        rxSpeed2, txSpeed2 = run_ifstat_during_test(INTERFACE2, DURATION)

        # Run GPS function
        lat, long = next(run_gps_during_test(PORT))

        # Capture iperf3 results 
        out, err = iperf_proc.communicate()
        bps = None
        if iperf_proc.returncode == 0:
            try:
                data = json.loads(out)
                bps = data['end']['sum_received']['bits_per_second']
            except Exception as e:
                print(f"Error parsing iperf3 JSON: {e}")

        if bps is not None and rxSpeed1 is not None and rxSpeed2 is not None:
            print(f"{timestamp} - {bps/1e6:.2f} Mbps | RX1: {rxSpeed1:.2f} Mbps TX1: {txSpeed1:.2f} Mbps | RX2: {rxSpeed2:.2f} Mbps TX2: {txSpeed2:.2f} Mbps | Lat: {lat:.4f} Long: {long:.4f}")
            f.write(f"{timestamp},{bps/1e6:.2f},{rxSpeed1:.2f},{txSpeed1:.2f},{rxSpeed2:.2f},{txSpeed2:.2f},{lat:.4f},{long:.4f}\n")
            f.flush()
        else:
            print(f"{timestamp} - Test failed.")

        time.sleep(PAUSE)
