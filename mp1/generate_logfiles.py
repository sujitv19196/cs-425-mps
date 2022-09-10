import os

LOGFILE_SIZE = 60 * (10 ** 6)

# Logfile 0 (infrequent grep results)
def create_logfile_0():
    f = open("logfiles/0.txt", "w")
    chars_written = 0
    for i in range(100):
        txt = "100 Hello World\n"
        f.write(txt)
        chars_written += len(txt)
    while (chars_written < LOGFILE_SIZE):
        txt = "Yippie!!!\n"
        f.write(txt)
        chars_written += len(txt)

# Logfile 1 (Only some servers will have it)
# Same text as logfile 0
def create_logfile_1():
    f = open("logfiles/1.txt", "w")
    chars_written = 0
    for i in range(100):
        txt = "100 Hello World\n"
        f.write(txt)
        chars_written += len(txt)
    while (chars_written < LOGFILE_SIZE):
        txt = "Yippie!!!\n"
        f.write(txt)
        chars_written += len(txt)

# Logfile 2 (frequent grep results)
def create_logfile_2():
    f = open("logfiles/2.txt", "w")
    chars_written = 0
    while (chars_written < LOGFILE_SIZE - 100000):
        txt = "100 Hello World\n"
        f.write(txt)
        chars_written += len(txt)
    while (chars_written < LOGFILE_SIZE):
        txt = "Yippie!!!\n"
        f.write(txt)
        chars_written += len(txt)

# Logfile 3 (made to execute regex commands)
def create_logfile_3():
    f = open("logfiles/3.txt", "w")
    chars_written = 0
    for i in range(100):
        txt = "Gray\n"
        f.write(txt)
        chars_written += len(txt)
    for i in range(100):
        txt = "Grey\n"
        f.write(txt)
        chars_written += len(txt)
    while (chars_written < LOGFILE_SIZE):
        txt = "Yippie!!!\n"
        f.write(txt)
        chars_written += len(txt)

create_logfile_0()
create_logfile_1()
create_logfile_2()
create_logfile_3()