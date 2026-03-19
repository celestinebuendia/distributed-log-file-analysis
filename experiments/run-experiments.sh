#!/bin/bash

GENERATOR="test-files/generated/TestFileGenerator.py"
PROCESSOR="main/log_processor.bin"
OUTPUT_DIR="test-files/generated"
RESULTS_DIR="experiments/results"
N_CORES=8

mkdir -p $OUTPUT_DIR $RESULTS_DIR

# Generate test files
echo "Generating test files..."
python3 $GENERATOR 1000000 $OUTPUT_DIR/test_log_1M.log
python3 $GENERATOR 3000000 $OUTPUT_DIR/test_log_3M.log
python3 $GENERATOR 5000000 $OUTPUT_DIR/test_log_5M.log
python3 $GENERATOR 10000000 $OUTPUT_DIR/test_log_10M.log
echo "Done generating."

# Run processor on generated files
echo "Running processor on generated files..."
for file in $OUTPUT_DIR/test_log_1M.log $OUTPUT_DIR/test_log_3M.log $OUTPUT_DIR/test_log_5M.log $OUTPUT_DIR/test_log_10M.log; do
    name=$(basename $file .log)
    for cores in 1 2 4 8; do
        echo "  Processing $name with $cores cores..."
        mpirun -np $cores $PROCESSOR $file > $RESULTS_DIR/${name}_cores${cores}.txt
    done
done

# Run processor on benchmark files
BENCHMARK_FILES=(
    "test-files/nasa/NASA_access_log_Aug95"
    "test-files/nasa/NASA_access_log_Jul95"
    "test-files/zanbil/access.log"
)
echo "Running processor on benchmark files..."
for file in "${BENCHMARK_FILES[@]}"; do
    name=${file#test-files/}
    name=${name//\//_}
    for cores in 1 2 4 8; do
        echo "  Processing $name with $cores cores..."
        mpirun -np $cores $PROCESSOR $file > $RESULTS_DIR/${name}_cores${cores}.txt
    done
done

echo "All done. Results in $RESULTS_DIR/"