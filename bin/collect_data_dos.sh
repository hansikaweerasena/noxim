#!/bin/bash

benchmarks=("FFT" "OCEAN" "RADIX" "FMM" "LU" "BARNES" "black")

# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."

    mkdir -p "dos/${benchmark}"

    # Copying the benchmark-specific file to data0.txt
    cp "64_${benchmark}_processed.txt" "data0.txt"

    # Inner loop for executing the command with varying traffic trace values
#    for offset in {0..63}; do
#        echo "Running ${benchmark} with traffic trace 1 ${offset}..."
#        ./noxim -config ../config_examples/test_config_dos.yaml > "wireless/64_8/${benchmark}/${offset}.txt"
#    done

    ./noxim -config ../config_examples/test_config_dos.yaml > "dos/${benchmark}.txt"

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."