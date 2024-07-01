#!/bin/bash

# variable in exp : no of mal nodes, pir of mal node, no of apps running, dir nodes location(FIXED), simulation cycles, mal nodes mapping,  app nodes mapping

#benchmarks=("FFT" "OCEAN" "RADIX" "FMM" "LU" "BARNES" "black")
benchmarks=("FFT" "OCEAN" "FMM" "black")

dir1=0
dir2=7
dir3=56
dir4=63


# Function to check if a number is in the excluded list
contains() {
    local n=$1
    shift
    for item; do
        [[ "$item" == "$n" ]] && return 0 # 0 => true
    done
    return 1 # 1 => false
}


# Loop through the array and run the command for each benchmark
for benchmark in "${benchmarks[@]}"; do
    echo "Preparing to run ${benchmark}..."

    mkdir -p "traces/${benchmark}"

    # Copying the benchmark-specific file to data0.txt
    cp "64_${benchmark}_processed_corner.txt" "data0.txt"

    # Inner loop for executing the command with varying traffic trace values
    for offset in {0..63}; do
        echo "Running ${benchmark} with traffic trace 1 ${offset}..."

        # selecting four random attacking nodes
        declare -a selected_numbers=()
        # Re-seed RANDOM using nanoseconds from date command
        sleep 1

        while [ ${#selected_numbers[@]} -lt 3 ]; do
            # Generate a random number between 0 and 63
            number=$((RANDOM % 64))

            # Check if this number is not one of the excluded ones
            if ! contains $number $offset $dir1 $dir2 $dir3 $dir4 "${selected_numbers[@]}"; then
                selected_numbers+=($number)
            fi
        done

        # Path to the input file and the output file
        input_file="t.txt"
        output_file="table_in.txt"

        # Make sure the output file is empty or create it
        > "$output_file"

        # Read input file and replace the first column
        index=0
        while IFS=' ' read -r col1 col2 col3
        do
            # Replace the first column with one of the selected numbers
            echo "${selected_numbers[$index]} $col2 $col3" >> "$output_file"
            let index+=1
        done < "$input_file"

        all_attackers=""

        for number in "${selected_numbers[@]}"; do
            all_attackers+="${number}_"
        done

        ./noxim -config ../config_examples/config_dos.yaml -traffic hybrid table_in.txt 1 ${offset} -sim 50000 > "traces/${benchmark}/A_${all_attackers}O${offset}.txt"
        > "$output_file"
        ./noxim -config ../config_examples/config_dos.yaml -traffic hybrid table_in.txt 1 ${offset} -sim 50000 > "traces/${benchmark}/N_O${offset}.txt"

    done

    # Revert the file name back to the original
    rm "data0.txt"

    echo "${benchmark} benchmarks completed."
done

echo "All benchmarks have been completed."