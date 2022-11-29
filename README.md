# exFAT File Reader

This is a program to read exFAT formatted files. An exFAT formatted volume can be used to test the file system reader.The program has three commands as follows:

### `info`

This command displays all the information about the exFAT file.

###  `list`

This command lists all the files and directories in the exFAT file.

### `get`

This command takes in the absolute path of a file. If the file exists, it creates a new file of the same name and contents.


# How to compile and run my program

Compile my program using the command `make`.  

Run my program as follows:

    ./main <exFAT_image> <commands> <absolutepath (only for the 'get' command)>

    For example:
    ./main exFATimage.exfat info
    ./main exFATimage.exfat list
    ./main exFATimage.exfat get ./text/numbers/1.txt

---

