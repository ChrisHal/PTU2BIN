
#####################
#####################
### (C) 2019 C. R. Halaszovich
### Tool to apply PTU2BIN to all PTU-files in current directory

import glob
import subprocess

defaultchannel = 2
toolcmd = "PTU2BIN"

filelist=glob.glob('*.ptu')
print("Found %i PTU files to convert."%len(filelist))
for fn in filelist:
	target = fn[:-4]+".bin"
	print("\n###############\nConverting %s to %s\n"%(fn,target))
	res=subprocess.run([toolcmd,fn,target,str(defaultchannel)])
	if res.returncode != 0:
		print("An error occured while converting file '%s'."%fn)
print("Press RETURN")
input()
