Copyright (c) 2019 Christian R. Halaszovich
(See LICENSE.txt for licensing information.)

Tools to convert PicoQuant PTU files (as written by e.g.
PicoQuant's SymphoTime64) to BIN files containing pre-histogrammed data.
This works for FLIM data in T3 format.
The number of supported formats is quite limited as of now. Fell free to
get in touch with me if you need addional formats supported. (PTU-files
can differ somewhat in the internal format they use.)

There are two tools provided:
PTU2BIN - This is the conversion tool an can be used to convert single files.
  (The original file is preserved, of course.) Addionally, some useful metadata is
  extracted from the PTU file.
convertPTUs.py - A Python script that batch-converts all ptu files in the current
  directory and its sub-directories and converts them using PTU2BIN.
  By default the output from PTU2BIN is written to individual txt-files. There
  is a command-line switch to send the output of PTU2BIN to the terminal instead.

BUILDING
Dependancies:
cxxopts.hpp (available at https://github.com/jarro2783/cxxopts.git)
This must be in your include path

Only the PTU2BIN executable needs to be build. The python script cna be used "as is".
Since in normal configuration only standard C++ is used building should work on
most systems. Instructions for Windows and Linux are privided:
On Windows: The preffered solution is to use VisualStudio and build using
  the VisualStudio solution file "PTU2BIN.sln".
On Linux: You will need a C++ compiler installed. We assume that the GNU compiler
  is used. Then "g++ -O2 -o  PTU2BIN PTU2BIN.cpp export_igor_ibw.cpp" should be
  sufficient to build the executable.

INSTALLING
Make sure that Python3.5 (or higher) is installed on your system.
Windows: The most convenient option is to place cenvertPTUs.py and PTU2BIN.exe in 
  a common directory. The files you want to convert should be in thw same directory
  or in its sub-directories.
Linux: Copy the executable PTU2BIN to a directory in your PATH,
  e.d. "/usr/local/bin/".

USAGE
Windows: Double-click "convertPTUs.py" -> all PTU files that are found in
  sub-directories will be converted.

Linux: In terminal, change to the directory where convertPTUs.py is located.
  Start the conversion using the command "python convertPTUs.py"

All OSs:
  To convert a single file:

  PTU2BIN <infile> <outfile> [<channel #>]

  <infile> must be in PTU format.   If <outfile> has extension ".ibw" a Igor
  binary file is written, otherwise a BIN file. For historical reasons,
  by default channel # 2 in the PTU file is evaluated. This can be overwritten
  by giving the number of the desired channel as the 3rd agument.
  Numbers <=0 indicate that all channels should be used.
  To learn about additional options:
  PTU2BIN --help
