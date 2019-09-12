
#####################
#####################
### (C) 2019 C. R. Halaszovich
### Tool to apply PTU2BIN to all PTU-files in current directory,
### including subdirectories

import os.path
import glob
import subprocess

defaultchannel = 2
toolcmd = "PTU2BIN"

filelist=glob.glob('**/*.ptu', recursive=True)
print("Found %i PTU files to convert."%len(filelist))
for fn in filelist:
    target = fn[:-4]+".bin"
    if os.path.exists(target):
        print("\nFile '%s' exists. Skipping conversion for this file."%target)
        continue
    print("\n###############\nconverting %s to %s"%(fn,target))
    res=subprocess.run([toolcmd,fn,target,str(defaultchannel)],capture_output=True,encoding="utf-8")
    if res.returncode != 0:
        print("An error occured while converting file '%s'."%fn)
        if(len(res.stderr)>0):
           print("ERROR: %s"%res.stderr)
    else:
        print("conversion finished")
        if(len(res.stdout)>0):
            textfilename=fn[:-4]+".txt"
            textfile=open(textfilename,"w")
            textfile.write(res.stdout)
            textfile.close()
            print("output has been written to txt-file")
print("Press RETURN")
input()
