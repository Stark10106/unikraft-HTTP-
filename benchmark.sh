#!/bin/bash

# Improved benchmark script for HTTP unikernel
echo "HTTP Server Unikernel Benchmark (Improved)"
echo "=========================================="
echo 

# Check if curl is available
if ! command -v curl &> /dev/null; then
    echo "Error: curl is not installed. Please install it to continue."
    exit 1
fi

# Function to measure response time with higher precision
measure_response() {
    # Use curl's built-in timing features for more accurate measurement
    result=$(curl -s -w "\nStatus: %{http_code}\nTime: %{time_total} seconds\nSize: %{size_download} bytes\n" http://localhost:8080/)
    
    # Extract the HTTP response body (everything before the Status line)
    body=$(echo "$result" | sed -n '1,/^Status:/p' | head -n -1)
    
    # Extract the metrics
    stats=$(echo "$result" | grep -E "Status:|Time:|Size:")
    
    # Output just the stats
    echo "$stats"
}

# Function to run serial requests with better timing
run_serial_test() {
    requests=$1
    echo "Running $requests serial requests..."
    
    total_time=0
    
    for i in $(seq 1 $requests); do
        echo -n "Request $i: "
        time_output=$(curl -s -w "%{time_total}" -o /dev/null http://localhost:8080/)
        total_time=$(echo "$total_time + $time_output" | bc)
        echo "$time_output seconds"
    done
    
    avg_time=$(echo "scale=6; $total_time / $requests" | bc)
    echo "Average response time: $avg_time seconds"
    echo
}

echo "Starting benchmark..." 

# Check if the server is running
if ! curl -s --connect-timeout 1 http://localhost:8080/ > /dev/null; then
    echo "Error: HTTP server is not running at localhost:8080."
    echo "Please make sure to start the unikernel with:"
    echo "kraft run --rm -p 8080:8080 --plat qemu --arch x86_64 --initrd .unikraft/build/initramfs-x86_64.cpio ."
    exit 1
fi

echo "Measuring initial response with detailed metrics..."
measure_response
echo

echo "Running serial test (5 requests)..."
run_serial_test 5
echo

echo "Simple load test (sequential, not parallel)..."
for i in {1..10}; do
    curl -s -o /dev/null http://localhost:8080/ &
done
wait
echo "Done!"
echo

echo "Final response measurement after load..."
measure_response
echo

echo "Benchmark complete."
echo
echo "Note: For this simple unikernel server, parallel benchmarking tools like"
echo "Apache Bench may not work well as the server has limited connection handling."
echo "The sequential tests above provide a better metric for this simple server."