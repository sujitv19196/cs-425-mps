import subprocess as sub
import time
import os

# Initialize tests by deleting all files in outputs directory
def reset_test():
    dir = "outputs"
    for f in os.listdir(dir):
        os.remove(os.path.join(dir, f))

def check_grep_result(original_filepath, output_filepath):
    output = open(original_filepath, "r").readlines()
    original = open(output_filepath, "r").readlines()
    ct = 0
    print("Output lines:", len(output))

    for output_line, original_line in zip(output, original):
        if (ct < 100 and output_line != original_line) :
            raise Exception("Test 0 failed. Grep outputs don't match expected!")
        ct += 1
    return True

# Test 0: Control   
def unit_test_0():
    # Run Client
    start_time = time.time()
    sub.run("./client grep 100 logfiles/0.txt", shell=True)
    run_time = time.time() - start_time

    # Verify Grep
    check_grep_result("logfiles/0.txt", "outputs/0.txt")

    # Return runtime
    print("Run time:", run_time)
    return run_time

# Test 1: Only some servers have specified logfile
# Same as unit test 0, except only 

# Test 2: Incorrect grep command
# Should not have any output files

reset_test()
unit_test_0()