Copyright (c) 2019 Christian R. Halaszovich
(See LICENSE.txt for licensing information.)

Tools to convert PicoQuant PTU files (as written by e.g. SymphoTime64) to
BIN files containing pre-histogrammed data. This works for FLIM data in
T3 format.

The number of supported formats is quite limited as of now. Fell free to
get in touch with me if you need addional formats supported. (PTU-files
can differ somewhat in the internal format they use.)

There are two tools provided:
PTU2BIN - This is the conversion tool an can be used to convert single files.
          (The original file is preserved, of course.) Some useful info is
		  put out by the tool.
convertPTUs.py - A Python script that batch-converts all ptu files in the current
                 directory and its sub-directories and converts them using
				 PTU2BIN. By default the output from PTU2BIN is written to
				 individual txt-files. There is a command-line switch to
				 send the output of PTU2BIN to the terminal instead.
