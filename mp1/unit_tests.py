import subprocess as sub
import time
import os
import numpy as np

VM_COUNT = 5

# Initialize tests by deleting all files in outputs directory
def reset_test():
    dir = "outputs"
    for f in os.listdir(dir):
        os.remove(os.path.join(dir, f))

# Check whether the grep result is correct
# Since the grep lines are always at the beginning of the file
# This function merely checks whether the first n lines of the grep output file
# Match the first n lines of the original log file
def check_grep_result(logfile_filepath, output_filepath, n):
    original = open(logfile_filepath, "r").readlines()
    output = open(output_filepath, "r").readlines()
    ct = 0

    for output_line, original_line in zip(output, original):
        if (ct < n and output_line != original_line) :
            raise Exception("Test failed. Grep outputs don't match expected!")
        ct += 1
    return True

# Test 0: Infrequent GREP 
# Use logfile 0  
def unit_test_0():
    print("Running test 0: Infrequent Grep. Only first 100 lines match.")

    # Run Client
    runtimes = []
    for i in range(5):
        print("Trial", i)
        start_time = time.time()
        sub.run("./client grep 100 logfiles/0.txt", shell=True)
        run_time = time.time() - start_time
        runtimes.append(run_time)

        # Verify Grep
        for i in range(VM_COUNT):
            check_grep_result(str.format("logfiles/0.txt", i), str.format("outputs/{}.txt", i), 100)
        print()

    # Print out stats
    print("Average runtime:", np.mean(runtimes))
    print("Standard deviation runtime:", np.std(runtimes))
    return run_time

# Test 1: Frequent GREP   
# Use logfile 2
def unit_test_1():
    print("Running test 1: Frequent Grep. First 3743750.")

    # Run Client
    runtimes = []
    for i in range(5):
        print("Trial", i)
        start_time = time.time()
        sub.run("./client grep 100 logfiles/2.txt", shell=True)
        run_time = time.time() - start_time
        runtimes.append(run_time)

        # Verify Grep
        for i in range(VM_COUNT):
            check_grep_result(str.format("logfiles/2.txt", i), str.format("outputs/{}.txt", i), 3743750)
        print()
        
    # Print out stats
    print("Average runtime:", np.mean(runtimes))
    print("Standard deviation runtime:", np.std(runtimes))
    return run_time

# Test 2: regex
# Use logfile 3
def unit_test_2():
    print("Running test 2: Regex Grep. First 200.")

    # Run Client
    runtimes = []
    for i in range(5):
        print("Trial", i)
        start_time = time.time()
        sub.run("./client \"grep \\\"Gr[a|e]y\\\" logfiles/3.txt\"", shell=True)
        run_time = time.time() - start_time
        runtimes.append(run_time)

        # Verify Grep
        for i in range(VM_COUNT):
            check_grep_result(str.format("logfiles/3.txt", i), str.format("outputs/{}.txt", i), 200)
        print()
        
    # Print out stats
    print("Average runtime:", np.mean(runtimes))
    print("Standard deviation runtime:", np.std(runtimes))
    return run_time

# Test 3: One server fails
# Use logfile 0
# Outputs from first 4 servers should be present, and not from the fifth
def unit_test_3():
    print("Running test 0: Infrequent Grep. Only first 100 lines match.")

    # Run Client
    runtimes = []
    for i in range(5):
        print("Trial", i)
        start_time = time.time()
        sub.run("./client grep 100 logfiles/0.txt", shell=True)
        run_time = time.time() - start_time
        runtimes.append(run_time)

        # Verify Grep
        for i in range(VM_COUNT - 1):
            check_grep_result(str.format("logfiles/0.txt", i), str.format("outputs/{}.txt", i), 100)
        print()

    # Print out stats
    print("Average runtime:", np.mean(runtimes))
    print("Standard deviation runtime:", np.std(runtimes))
    return run_time

reset_test()
# unit_test_0()
# unit_test_1()
# unit_test_2()
unit_test_3()