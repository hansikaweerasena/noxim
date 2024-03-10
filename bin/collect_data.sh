#!/bin/bash

benchmarks=("FFT" "FMM" "LU" "BARNES" "OCEAN" "RADIX" "black")

benchmarks=("FFT" "OCEAN" "RADIX")

# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."

    mkdir -p "wireless/${benchmark}"

    # Copying the benchmark-specific file to data0.txt
    cp "64_${benchmark}_processed.txt" "data0.txt"

    # Inner loop for executing the command with varying traffic trace values
    for offset in {0..63}; do
        echo "Running ${benchmark} with traffic trace 1 ${offset}..."
        ./noxim -config ../config_examples/default_config_wireless_only_64.yaml -traffic trace 1 ${offset} > "wireless/${benchmark}/${offset}.txt"
    done

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."
