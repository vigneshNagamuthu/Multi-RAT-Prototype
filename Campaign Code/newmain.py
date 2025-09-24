import json
import subprocess
import time
import threading
import os
from datetime import datetime
from zoneinfo import ZoneInfo
from ifstat_utils2 import run_ifstat_during_test  
from gps_utils import run_gps_during_test

# Int 1 - Singtel, Int 2 - Simba
INTERFACE2 = "wlp2s0"
INTERFACE1 = "wlxe0e1a9195466"

PORT = "/dev/ttyACM0"

AWS_SERVER = "47.128.71.167"
DURATION = 10
PAUSE = 2

start_time_str = datetime.now(ZoneInfo("Asia/Singapore")).strftime("%Y%m%d_%H%M%S")
OUTPUT_FILE = f"iperf3_results_{start_time_str}.csv"

def check_gps_available():
    """Check if GPS device is available"""
    return os.path.exists(PORT) and os.access(PORT, os.R_OK)

def safe_parse_iperf3_output(output):
    """Safely parse iperf3 JSON output and extract relevant metrics"""
    try:
        data = json.loads(output)
        
        # For client receiving data (download test), we want sum_received
        # This represents the total data received by the client
        if 'end' in data and 'sum_received' in data['end']:
            received_bps = data['end']['sum_received']['bits_per_second']
            return received_bps
        
        # Fallback: try to get from sum (total)
        elif 'end' in data and 'sum' in data['end']:
            if 'bits_per_second' in data['end']['sum']:
                return data['end']['sum']['bits_per_second']
            elif 'receiver' in data['end']['sum']:
                return data['end']['sum']['receiver']['bits_per_second']
        
        # Additional fallback for different iperf3 output formats
        elif 'end' in data and 'streams' in data['end'] and len(data['end']['streams']) > 0:
            # Sum up all streams if multiple
            total_bps = 0
            for stream in data['end']['streams']:
                if 'receiver' in stream and 'bits_per_second' in stream['receiver']:
                    total_bps += stream['receiver']['bits_per_second']
                elif 'bits_per_second' in stream:
                    total_bps += stream['bits_per_second']
            if total_bps > 0:
                return total_bps
        
        print("Warning: Could not find bits_per_second in iperf3 output")
        return None
        
    except json.JSONDecodeError as e:
        print(f"Error parsing iperf3 JSON: {e}")
        print(f"Raw output: {output}")
        return None
    except KeyError as e:
        print(f"Error accessing iperf3 data structure: {e}")
        print(f"Available keys: {data.keys() if 'data' in locals() else 'N/A'}")
        return None
    except Exception as e:
        print(f"Unexpected error parsing iperf3 output: {e}")
        return None

# Check GPS availability at startup
gps_available = check_gps_available()
if not gps_available:
    print(f"Warning: GPS device {PORT} not found or not accessible. GPS coordinates will be set to 0.0")

with open(OUTPUT_FILE, "w") as f:
    # Keep your original CSV format
    f.write("timestamp,iperf3_download_Mbps,RX1_Mbps,TX1_Mbps,RX2_Mbps,TX2_Mbps,Latitude,Longitude\n")

    iteration = 0
    while True:
        iteration += 1
        timestamp = datetime.now(ZoneInfo("Asia/Singapore")).strftime("%H:%M:%S")
        print(f"\n--- Iteration {iteration} at {timestamp} ---")

        # Storage for results with thread-safe access
        results = {
            "rx1": None, "tx1": None, 
            "rx2": None, "tx2": None, 
            "lat": None, "lon": None, 
            "iperf3_bps": None
        }
        
        # Lock for thread-safe access to results
        results_lock = threading.Lock()

        # Thread functions with improved error handling
        def iperf_task():
            try:
                print("Starting iperf3 test...")
                iperf_cmd = ["mptcpize", "run", "iperf3", "-c", AWS_SERVER, "-t", str(DURATION), "-J"]
                proc = subprocess.Popen(iperf_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
                out, err = proc.communicate()
                
                if proc.returncode == 0:
                    bps = safe_parse_iperf3_output(out)
                    with results_lock:
                        results["iperf3_bps"] = bps
                    if bps is not None:
                        print(f"iperf3 completed: {bps/1e6:.2f} Mbps")
                    else:
                        print("iperf3 completed but failed to parse output")
                else:
                    print(f"iperf3 command failed with return code {proc.returncode}")
                    if err:
                        print(f"stderr: {err}")
                    with results_lock:
                        results["iperf3_bps"] = None
                        
            except FileNotFoundError:
                print("Error: mptcpize or iperf3 command not found")
                with results_lock:
                    results["iperf3_bps"] = None
            except Exception as e:
                print(f"Error in iperf3 task: {e}")
                with results_lock:
                    results["iperf3_bps"] = None

        def ifstat_task1():
            try:
                print(f"Starting ifstat for interface {INTERFACE1}...")
                rx, tx = run_ifstat_during_test(INTERFACE1, DURATION)
                with results_lock:
                    results["rx1"], results["tx1"] = rx, tx
                print(f"Interface {INTERFACE1}: RX={rx:.2f} TX={tx:.2f} Mbps")
            except Exception as e:
                print(f"Error in ifstat task 1 ({INTERFACE1}): {e}")
                with results_lock:
                    results["rx1"], results["tx1"] = None, None

        def ifstat_task2():
            try:
                print(f"Starting ifstat for interface {INTERFACE2}...")
                rx, tx = run_ifstat_during_test(INTERFACE2, DURATION)
                with results_lock:
                    results["rx2"], results["tx2"] = rx, tx
                print(f"Interface {INTERFACE2}: RX={rx:.2f} TX={tx:.2f} Mbps")
            except Exception as e:
                print(f"Error in ifstat task 2 ({INTERFACE2}): {e}")
                with results_lock:
                    results["rx2"], results["tx2"] = None, None

        def gps_task():
            try:
                if not gps_available:
                    # GPS not available, use default coordinates
                    with results_lock:
                        results["lat"], results["lon"] = 0.0, 0.0
                    return
                
                print("Starting GPS reading...")
                # Create a fresh GPS generator for each iteration
                gps_gen = run_gps_during_test(PORT)
                lat, lon = next(gps_gen)
                with results_lock:
                    results["lat"], results["lon"] = lat, lon
                print(f"GPS coordinates: {lat:.4f}, {lon:.4f}")
            except Exception as e:
                print(f"Error in GPS task: {e}")
                with results_lock:
                    results["lat"], results["lon"] = 0.0, 0.0

        # Start all threads
        threads = [
            threading.Thread(target=iperf_task, name="iperf3"),
            threading.Thread(target=ifstat_task1, name="ifstat1"),
            threading.Thread(target=ifstat_task2, name="ifstat2"),
            threading.Thread(target=gps_task, name="gps"),
        ]
        
        print("Starting all measurement threads...")
        for t in threads:
            t.start()
        
        # Wait for all threads to complete
        for t in threads:
            t.join()
        
        print("All threads completed.")

        # Print & save results with better formatting and error handling
        with results_lock:
            # Convert iperf3 result to Mbps
            iperf3_mbps = results['iperf3_bps'] / 1e6 if results['iperf3_bps'] is not None else 0.0
            
            # Use 0.0 for any None values
            rx1 = results['rx1'] if results['rx1'] is not None else 0.0
            tx1 = results['tx1'] if results['tx1'] is not None else 0.0
            rx2 = results['rx2'] if results['rx2'] is not None else 0.0
            tx2 = results['tx2'] if results['tx2'] is not None else 0.0
            lat = results['lat'] if results['lat'] is not None else 0.0
            lon = results['lon'] if results['lon'] is not None else 0.0
            
            # Print summary
            print(f"\n{timestamp} - RESULTS:")
            print(f"Download: {iperf3_mbps:.2f} Mbps")
            print(f"Int1 ({INTERFACE1}) - RX: {rx1:.2f} TX: {tx1:.2f} Mbps")
            print(f"Int2 ({INTERFACE2}) - RX: {rx2:.2f} TX: {tx2:.2f} Mbps")
            print(f"GPS: {lat:.4f}, {lon:.4f}")
            
            # Always write data (with 0.0 for failed tests)
            f.write(f"{timestamp},{iperf3_mbps:.2f},{rx1:.2f},{tx1:.2f},"
                    f"{rx2:.2f},{tx2:.2f},{lat:.4f},{lon:.4f}\n")
            f.flush()

        print(f"Pausing for {PAUSE} seconds...")
        time.sleep(PAUSE)
