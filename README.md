# ARP_Assignment3
Farah Yassin 4801788

## Description
This third *Advanced and Robot Programming* (ARP) assignment consists in enhancing the [second assignment](https://github.com/yassinfrh/ARP-Assignment2) by adding client/server functionalities. In particular, the user can choose to run the program in normal, server or client mode and, in case of client or server mode, use TCP protocol sockets to remotely command the `processA`.

## Folders content
The repository is organized as follows:
- the `src` folder contains the source code for all the processes
- the `include` folder contains all the data structures and methods used within the ncurses framework to build the two GUIs

After compiling the program other two directories will be created:

- the `bin` folder contains all the executable files
- the `out` folder will contain the saved image as a *bmp* file

## Processes
The program is composed of 3 processes:
-  `master.c`, in addition to the features implemented in the [second assignment](https://github.com/yassinfrh/ARP-Assignment2), will ask the user in which modality to run the program, normal, server or client, and launch the two processes.
-  `processA.c`, depending on how the user launched the program, will work as follows:
    - in **normal** mode the program will work the same as in the second assignment 
    - in **server** mode the program will wait until a client is connected and listen for inputs from the client to move the circle in the window
    - in **client** mode the program will connect to a server and command both its window and the server window, by sending via socket the pressed keys
-  `processB.c` will work as in the second assignment, depending on the position of the circle of the `processA`

## Requirements
The program requires the installation of the **konsole** program, of the **ncurses** library and of the **bitmap** library. To install the konsole program, simply open a terminal and type the following command:
```console
$ sudo apt-get install konsole
```
To install the ncurses library, type the following command:
```console
$ sudo apt-get install libncurses-dev
```

To install the bitmap library, you need to follow these steps:
1. Download the source code from [this GitHub repo](https://github.com/draekko/libbitmap.git) in your file system.
2. Navigate to the root directory of the downloaded repo and run the configuration through command ```./configure```. Configuration might take a while.  While running, it prints some messages telling which features it is checking for.
3. Type ```make``` to compile the package.
4. Run ```make install``` to install the programs and any data files and documentation.
5. Upon completing the installation, check that the files have been properly installed by navigating to ```/usr/local/lib```, where you should find the ```libbmp.so``` shared library ready for use.
6. In order to properly compile programs which use the *libbitmap* library, you first need to notify the **linker** about the location of the shared library. To do that, you can simply add the following line at the end of your ```.bashrc``` file:      
```export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"```

## Compiling and running the code
Two shell scripts have been provided to compile and run the code. To compile the code simply open a terminal from inside the directory and type the following command:
```console
$ bash compile.sh
```
To run the code type the following command:
```console
$ bash run.sh
```
After running the code, the user will be asked in which modality to launch the program:

    Which modality do you want to launch the program in (insert the number)?
    1. Normal
    2. Server
    3. Client

Insert the number to choose the modality. In case you choose to run the program in **client** mode, you'll be asked to write the IP address of the server and the port number to use

    Insert the IP address:
    <ip_address>

    Insert the port number (between 1024 and 65535):
    <port_number>

If you choose to run the program in **server** mode, you'll be asked just for the port number.

To work properly, the window related to `processA.c` needs to be 90x30 and the window related to `processB.c` needs to be 80x30.