benchmarks=("FFT" "OCEAN" "RADIX" "FMM" "LU" "BARNES" "black")

> "output.txt"

# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."


    # Copying the benchmark-specific file to data0.txt
    cp "64_${benchmark}_processed.txt" "data0.txt"

    for benchmark2 in "${benchmarks[@]}"; do


        # Copying the benchmark-specific file to data1.txt
        cp "64_${benchmark2}_processed.txt" "data1.txt"
        # Run x number of trials
        for offset in {0..1}; do

            # Generate a random number between 0 and 63
            number=$((RANDOM % 64))

            echo "Running ${benchmark} and ${benchmark2} with traffic trace 2 ${number}..." >> "output.txt"

              ./noxim -config ../config_examples/default_config.yaml -traffic trace 2 ${number} >> "output.txt"
        
        done

        # Revert the file name back to the original
        rm "data1.txt"
    done

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."