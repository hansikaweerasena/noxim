#!/bin/bash

benchmarks=("FFT" "OCEAN" "RADIX" "FMM" "LU" "BARNES" "black")

# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."

    mkdir -p "wireless/4_4/${benchmark}"

    # Copying the benchmark-specific file to data0.txt
    cp "4_${benchmark}_processed.txt" "data0.txt"

    # Inner loop for executing the command with varying traffic trace valuess
    for offset in {0..3}; do
        echo "Running ${benchmark} with traffic trace 1 ${offset}..."
        ./noxim -config ../config_examples/default_config_wireless_only.yaml -traffic trace 1 ${offset} > "wireless/4_4/${benchmark}/${offset}.txt"
    done

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."