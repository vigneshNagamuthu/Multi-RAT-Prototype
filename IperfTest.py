import subprocess
import json
import time
from datetime import datetime
from zoneinfo import ZoneInfo

INTERFACE = "wlp87s0f0"   # your network interface
AWS_SERVER = "47.129.143.46"
DURATION = 20
PAUSE = 2
OUTPUT_FILE = "iperf3_results.csv"

def run_ifstat_during_test(duration):
    """Run ifstat for `duration` seconds and return avg RX/TX in Mbps."""
    cmd = ["ifstat", "-i", INTERFACE, "1", str(duration)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    lines = result.stdout.strip().splitlines()
    data_lines = lines[2:]  # skip header lines

    rx_values, tx_values = [], []
    for line in data_lines:
        parts = line.split()
        if len(parts) >= 2:
            try:
                rx_kBps, tx_kBps = float(parts[0]), float(parts[1])
                rx_values.append(rx_kBps)
                tx_values.append(tx_kBps)
            except ValueError:
                continue

    if not rx_values:
        return None, None

    avg_rx_mbps = (sum(rx_values) / len(rx_values)) * 8 / 1000
    avg_tx_mbps = (sum(tx_values) / len(tx_values)) * 8 / 1000
    return avg_rx_mbps, avg_tx_mbps

with open(OUTPUT_FILE, "w") as f:
    f.write("timestamp,iperf3_Mbps,RX_Mbps,TX_Mbps\n")

    while True:
        timestamp = datetime.now(ZoneInfo("Asia/Singapore")).strftime("%H:%M:%S")

        # Start iperf3 as subprocess
        iperf_cmd = ["mptcpize", "run", "iperf3", "-c", AWS_SERVER, "-t", str(DURATION), "-J"]
        iperf_proc = subprocess.Popen(iperf_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

        # Run ifstat for same duration in parallel
        rxSpeed, txSpeed = run_ifstat_during_test(DURATION)

        # Wait for iperf3 to finish
        out, err = iperf_proc.communicate()
        bps = None
        if iperf_proc.returncode == 0:
            try:
                data = json.loads(out)
                bps = data['end']['sum_received']['bits_per_second']
            except Exception as e:
                print(f"Error parsing iperf3 JSON: {e}")

        if bps is not None and rxSpeed is not None:
            print(f"{timestamp} - {bps/1e6:.2f} Mbps | RX: {rxSpeed:.2f} Mbps TX: {txSpeed:.2f} Mbps")
            f.write(f"{timestamp},{bps/1e6:.2f},{rxSpeed:.2f},{txSpeed:.2f}\n")
            f.flush()
        else:
            print(f"{timestamp} - Test failed.")

        time.sleep(PAUSE)
