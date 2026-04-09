# api-bomber
A Fast, Light C++ Utility to Stress test your APIs on localhost. For learning OS concepts and LLD patterns.

- Compatible with web based pipelines by utilizing json config and output files.
- Multithreaded api bomber with mutex locks to prevent deadlocks
- Proper analytics with Success Rate, Average, 95th percentile, 99th percentile of various latency parameters like server latency, tcp handshake etc.

Scroll below for setup and usage guide.

## Why build this?

I wanted to go from a C++ Coder who only uses it on leetcode to a C++ Developer. Learnings :

- Multi-Threading in CPP
- Manager-Worker LLD Pattern
- OS fundamentals
- CPP project best practices

## How does this Work?

The main flow of this project is the Manager Worker Pattern. 

### Manager (Main Thread)

It is simply the main thread. This orcehstrates the entire operation, configuration loading, worker thread creation, calculations and data export.

### Workers (Worker Threads)

Workers have one single task : Hit the specified API endpoint, in the requested amount. To prevent race conditions, while storing the `responseTimes`, the `responseTimes` data structure is protected by a mutex lock using `std::mutex`. 

To increase efficiency and prevent time wastage in locking and unlocking, worker threads accumulate results in a temporary data structure, and then modifying the global data structure only once, hence needing only one lock-unlock cycle.


`libcurl` is used under the hood to make api calls and getting fine tuned measurements.

```
    [Worker Thread]
        |
        V
    (Execute API Calls)
        |
        V
    (Accumulate in Temporary Data Structure)
        |
        V
    (Attempt to acquire mutex lock)
        |
        | Enters Critical Section
        V
    (On lock success, write to global data structure)
        |
        V
    (Release Mutex Lock)
        |
        | Leaving Critical Section
        V
    Terminate
```

## Prerequisites
To build and run API Bomber, you will need:
* A C++17 compatible compiler (e.g., GCC/MinGW)
* `make`
* `libcurl` development libraries

## Installation & Build

Clone the repository and use the provided `Makefile` to compile the project:

```bash
make
```
## Configuration

Before running the tool, create a bomber-config.json file in the same directory as the executable. This file dictates the behavior of the load test.

Example bomber-config.json:
```JSON
{
    "url": "http://localhost:4000/api/health",
    "concurrent_requests": 10,
    "requests_per_thread": 1000
}
```

- `url`: The target API endpoint you want to test.
- `concurrent_requests`: The number of parallel OS threads to spawn (simulating concurrent users).
- `requests_per_thread`: How many sequential requests each thread should make. (Total Requests = concurrent_requests * requests_per_thread)

## Usage
Once compiled and configured, simply execute the binary:

```Bash
./api-bomber
```

The tool will parse your configuration, execute the load test, print live metrics to the console, and automatically generate a results file formatted as bomber-run-<timestamp>.json in your current directory.

