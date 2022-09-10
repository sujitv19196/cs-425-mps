import os

LOGFILE_SIZE = 60 * (10 ** 6)

# Logfile 0
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

create_logfile_0()
create_logfile_1()