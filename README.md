
# PTU2BIN

Copyright (c) 2021 Christian R. Halaszovich
(See LICENSE.txt for licensing information.)

## Introduction
Tools to convert PicoQuant PTU files (as written by e.g.
PicoQuant's SymphoTime64) to BIN files or IgorPro binary wave files (IBW)
containing pre-histogrammed data.

This works for T3-mode FLIM data in. The the file must have been recorded in "Image" mode,
"Point" or "Line" modes are not supported.

It has only been tested with TimeHarp260P and PicoHarp data
so far. Feel free to get in touch with me if you need additional formats supported. Support
for T2 mode is planned.

There are two tools provided:

* `PTU2BIN` - This is the conversion tool that can be used to convert single PTU files.
(The original file is preserved, of course.) Additionally, some useful metadata is
extracted from the PTU file.

* `convertPTUs.py` - A Python script that batch-converts all `.ptu` files in the current
directory and its sub-directories using PTU2BIN.
By default the output from PTU2BIN is written to individual text files. There
is a command-line switch to send the output of PTU2BIN to the terminal instead.


## BUILDING

Only the PTU2BIN executable needs to be build. The python script can be used "as is".
Only standard C++17 is used, so building should work on
most systems. Instructions for Windows and Linux are provided.

**External dependencies:**
`cxxopts.hpp` (available at https://github.com/jarro2783/cxxopts.git).
Place this in the "PTU2BIN/external/" directory.
PTU2BIN was build using version 2.2.0 of cxxopts, newer versions might contain
breaking changes.

### Windows

The recommended way is to use VisualStudio and build using
the VisualStudio solution file `PTU2BIN.sln`. Currently, VisualStudio 2019 ist used to
develope and test PTU2BIN.

### Linux

(This should also work on other platforms.)

The build process uses `cmake`. You will need a C++ compiler installed.

In top directory, create a build directory (this will sit parallel to "PTU2BIN" in the top directory,
which might also be called "PTU2BIN"):

``mkdir PTU2BIN_build``

Change to that directroy:

``cd PTU2BIN_build``

Execute these commands (for "Release" type build):  
``cmake -DCMAKE_BUILD_TYPE=Release ../PTU2BIN``  
``cmake --build .``


## INSTALLATION

Make sure that Python3.5 (or higher) is installed on your system.

### Windows
The most convenient option is to place `convertPTUs.py` and `PTU2BIN.exe` in 
 a common directory. The files you want to convert should be in the same directory
 or in its sub-directories. (Altermatively, your might place these two files somwhere in your path.))

### Linux

Make the Python script executable (using command `chmod +x myscript.py`).

Copy the executable `PTU2BIN` and the Python script `convertPTUs.py` (found in directory
`convertPTUs`) to a directory in your PATH,
e.g. `/usr/local/bin/`.

## USAGE

### Batch-Processing

To convert all files in current directory and subdirectories:

**Windows**:
Double-click `convertPTUs.py` -> all PTU files that are found in
sub-directories, starting from the location of `convertPTUs.py` will be converted.
Alternatively, if you installed in your `PATH`, you can use the method described in the next paragraph.

**Linux**:
In terminal, change to the directory where convertPTUs.py is located.
Start the conversion using the command `convertPTUs.py`.


By default, the output of the conversion tool for each converted `<name>.ptu` file will be written to
a `<name>.txt` file. Use option `-d` to direct this output to the terminal.

To learn about additional features, execute

`convertPTUs.py -h`

### Converting individual files

**All OSs:** It is assumed that `PTU2BIN` is installed in your `path`.

To convert a single file:

`PTU2BIN <infile> <outfile> [<channel #>]`

`<infile>` must be in PTU format.

If `<outfile>` has extension `.ibw`, an Igor
binary file is written, otherwise a `BIN` file.

The created file will contain data from one individual detector channel
or the sum of data from all channels. *For historical reasons,
channel # 2 is evaluated by default*. This can be overwritten
by giving the number of the desired channel as the 3rd argument.
Numbers <=0 indicate that all channels should be used, i.e. the photon counts of all
channels will be summed together.

To learn about additional options:

`PTU2BIN --help`



