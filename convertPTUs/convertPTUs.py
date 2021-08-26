
#####################
#####################
### (C) 2021 C. R. Halaszovich
### Tool to apply PTU2BIN to all PTU-files in current directory,
### including subdirectories

import os.path
import sys
import getopt
import glob
import subprocess

defaultchannel = 2
capturetofile=True
toolcmd = "PTU2BIN"

helpmsg = """
Usage: convertPTUs.py [-c <channel#> ] [-d] [-h|--help]
-c <number>: analyse channel with given number (default: 2) (<=0 for all channels)
-d: display output of conversion tool (instead of writing it to a file)
-h: print this message
"""

try:
    opts, args = getopt.getopt(sys.argv[1:], "c:dh",["help"])
except getopt.GetoptError as err:
    # print help information and exit:
    print(err) # will print something like "option -a not recognized"
    print(helpmsg)
    print("Press RETURN")
    input()
    sys.exit(2)
for o, a in opts:
    if o == "-c":
        defaultchannel=int(a)
    elif o == '-d':
        capturetofile = False
    elif o in ('-h', '--help'):
        print(helpmsg)
        print("Press RETURN")
        input()
        sys.exit(0)

filelist=glob.glob('**/*.ptu', recursive=True)
print("Found %i PTU files to convert."%len(filelist))
for fn in filelist:
    target = fn[:-4]+".bin"
    if os.path.exists(target):
        print("\nFile '%s' exists. Skipping conversion for this file."%target)
        continue
    print("\n###############\nconverting %s to %s"%(fn,target))
    res=subprocess.run([toolcmd,fn,target,str(defaultchannel)],capture_output=capturetofile,encoding="utf-8")
    if res.returncode != 0:
        print("An error occured while converting file '%s'."%fn)
        if res.stderr is not None:
           print("ERROR: %s"%res.stderr)
    else:
        print("conversion finished")
        if res.stdout is not None:
            textfilename=fn[:-4]+".txt"
            textfile=open(textfilename,"w")
            textfile.write(res.stdout)
            textfile.close()
            print("output has been written to txt-file")
print("Press RETURN")
input()
