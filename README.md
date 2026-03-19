# Distributed Log File Analysis
An MPI-driven distributed web server log file analysis system, implemented with Open MPI in C++. 

## Prerequisites

### Log Processor

#### Install Open MPI

On Ubuntu: 
```sudo apt install openmpi-bin libopenmpi-dev```

On MacOS: 
```brew install open-mpi```

### Test File Generator

#### Install Faker on Python

```python -m pip install faker```

## Testing Logs

### NASA-HTTP

Two month log files (July and August 1995) of HTTP requests to the NASA Kennedy Space Center website (1,891,715 and 1,569,898 lines).

**Source:** https://ita.ee.lbl.gov/html/contrib/NASA-HTTP.html

### Zanbil

One month log file (January 2019) of HTTP requests to the Iranian web store Zanbil (10,365,152 lines).

**Source:** https://www.kaggle.com/datasets/eliasdabbas/web-server-access-logs