benchmarks=("FFT" "OCEAN" "RADIX" "FMM" "LU" "BARNES" "black")

> "output.txt"

# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."


    # Copying the benchmark-specific file to data0.txt
    cp "64_${benchmark}_processed.txt" "data0.txt"

    # Run x number of trials
    for offset in {0..9}; do

        # Generate a random number between 0 and 63
        number=$((RANDOM % 64))

        echo "Running ${benchmark} with traffic trace 1 ${number}..." >> "output.txt"

          ./noxim -config ../config_examples/default_config.yaml -traffic trace 1 ${number} >> "output.txt"
        
    done

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."