# Distributed Log File Analysis
An MPI-driven distributed web server log file analysis system for the batch processing of Common Log Format (CLF), Combined Log Format, and Combined Log Format + Response Time log files, implemented with Open MPI in C++. 

## Directory Structure

```
.
├── experiments/                    # Running pre-set experiment configuration
│   ├── results/                    # Output directory for experiment outputs
|   └── run-experiments.sh          # Script to run pre-set experiments
├── main/
│   └── log_processor.cpp           # Main distributed log processing program
└── test-files/                     # Experiment test files
    ├── generated/                  # Generated test files
    |   └── TestFileGenerator.py    # Script to generate synthetic test files
    ├── nasa/                       # Create, download NASA-HTTP, and insert here
    └── zanbil/                     # Create, download Zanbil, and insert here
```

Note that `nasa` and `zanbil` folders could not be committed because of log file size. Follow the below test file links to download and insert test files into repo if you wish to run the pre-set experiment.

Also note that the `results` folder currently holds experiment results analyzed in the final report of this project.

## Prerequisites

### Main Log Processor

#### Install Open MPI

On Ubuntu: 
```sudo apt install openmpi-bin libopenmpi-dev```

On MacOS: 
```brew install open-mpi```

### Test File Generator

#### Install Faker on Python

```python -m pip install faker```

## Usage

### Main Log Processor

```bash
mpic++ -std=c++20 -o ./main/log_processor.bin ./main/log_processor.cpp
mpirun -np <number_of_ranks> ./main/log_processor.bin <path_to_log_file>
```

### Test File Generator

```bash
python3 ./test-files/generated/TestFileGenerator.py <number_of_lines> <path_to_output_file>
```

### Pre-Set Generations and Tests

```bash
chmod +x experiments/run-experiments.sh
./experiments/run-experiments.sh
```

## Testing Logs

### Generated

Test log files generated of specified line length with randomization, using `TestFileGenerator.py`. For the set experiment, the four files of lengths 1M, 3M, 5M, and 10M are generated. In Combined Log Format with an extra field for response time.

### NASA-HTTP

Two month log files (July and August 1995) of HTTP requests to the NASA Kennedy Space Center website (1,891,715 and 1,569,898 lines). In CLF.

**Source:** https://ita.ee.lbl.gov/html/contrib/NASA-HTTP.html

### Zanbil

One month log file (January 2019) of HTTP requests to the Iranian web store Zanbil (10,365,152 lines). In Combined Log Format with one extra field.

**Source:** https://www.kaggle.com/datasets/eliasdabbas/web-server-access-logs