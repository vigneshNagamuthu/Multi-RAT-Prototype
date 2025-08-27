# ifstat_utils.py
import subprocess

def run_ifstat_during_test(interface, duration):
    """
    Run ifstat for `duration` seconds on a given interface.
    Returns average RX/TX in Mbps.
    """
    cmd = ["ifstat", "-i", interface, "1", str(duration)]
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
