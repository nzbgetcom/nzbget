## Configuration

NZBGet needs a configuration file. 

An example configuration file is provided in "nzbget.conf", which
is installed into "`<prefix>`/share/nzbget" (where `<prefix>` depends on
system configuration and configure options - typically "/usr/local",
"/usr" or "/opt"). The installer adjusts the file according to your
system paths. If you have performed the installation step 
"make install-conf" this file is already copied to "`<prefix>`/etc" and
NZBGet finds it automatically. If you install the program manually
from a binary archive you have to copy the file from "`<prefix>`/share/nzbget"
to one of the locations listed below.

Open the file in a text editor and modify it accodring to your needs.

You need to set at least the option "MAINDIR" and one news server in
configuration file. The file has comments on how to use each option.

The program looks for configuration file in following standard 
locations (in this order):

On POSIX systems:
```
<EXE-DIR>/nzbget.conf
~/.config/nzbget.conf
/etc/nzbget.conf
```

On Windows:
```
<EXE-DIR>\nzbget.conf
```

If you put the configuration file in other place, you can use command-
line switch "-c `<filename>`" to point the program to correct location.

In special cases you can run program without configuration file using
switch "-n". You need to use switch "-o" to pass required configuration 
options via command-line.

## Usage

NZBGet can be used in either standalone mode which downloads a single file 
or as a server which is able to queue up numerous download requests.

TIP for Windows users: NZBGet is controlled via various command line
parameters. For easier using there is a simple shell script included
in "nzbget-shell.bat". Start this script from Windows Explorer and you will
be running a command shell with PATH adjusted to find NZBGet executable.
Then you can type all commands without full path to nzbget.exe.

### Standalone mode:
--------------------
```
nzbget <nzb-file>
```
### Server mode:
----------------

First start the nzbget-server:

  - in console mode:
```
nzbget -s
```
  - or in daemon mode (POSIX only):
```
 nzbget -D
```	
  - or as a service (Windowx only, firstly install the service with command "nzbget -install"):
```
  net start NZBGet
``` 
To stop server use:
```
nzbget -Q  
```
TIP for POSIX users: with included script "nzbgetd" you can use standard
commands to control daemon:
```
nzbgetd start
nzbgetd stop
```

When NZBGet is started in console server mode it displays a message that
it is ready to receive download requests. In daemon mode it doesn't print any
messages to console since it runs in background.

When the server is running it is possible to queue up downloads. This can be
done either in terminal with "nzbget -A <nzb-file>" or by uploading
a nzb-file into server's monitor-directory (<MAINDIR>/nzb by default).

To check the status of server start client and connect it to server:
```
nzbget -C
```
The client have three different (display) outputmodes, which you can select
in configuration file (on client computer) or in command line. Try them:
```
nzbget -o outputmode=log -C
nzbget -o outputmode=color -C
nzbget -o outputmode=curses -C
```
To list files in server's queue:
```
nzbget -L
```
It prints something like:
```
[1] nzbname\filename1.rar (50.00 MB)
[2] nzbname\filename1.r01 (50.00 MB)
[3] another-nzb\filename3.r01 (100.00 MB)
[4] another-nzb\filename3.r02 (100.00 MB)
```
This is the list of individual files listed within nzb-file. To print
the list of nzb-files (without content) add G-modifier to the list command:
```
[1] nzbname (4.56 GB)
[2] another-nzb (4.20 GB)
```
The numbers in square braces are ID's of files or groups in queue.
They can be used in edit-command. For example to move file with
ID 2 to the top of queue:
```
nzbget -E T 2
```  
or to pause files with IDs from 10 to 20:
```
nzbget -E P 10-20
```
or to delete files from queue:
```
nzbget -E D 3 10-15 20-21 16
```

The edit-command has also a group-mode which affects all files from the
same nzb-file. You need to pass an ID of the group. For example to delete
the whole group 1:
```
nzbget -E G D 1
```
The switch "o" is useful to override options in configuration files. 
For example:
```
nzbget -o reloadqueue=no -o dupecheck=no -o parcheck=yes -s
```  
or:
```
nzbget -o createlog=no -C
```

### Running client & server on seperate machines:
-------------------------------------------------

Since nzbget communicates via TCP/IP it's possible to have a server running on
one computer and adding downloads via a client on another computer.

Do this by setting the "ControlIP" option in the nzbget.conf file to point to the
IP of the server (default is localhost which means client and server runs on 
same computer)

### Security warning
--------------------

NZBGet communicates via unsecured socket connections. This makes it vulnerable.
Although server checks the password passed by client, this password is still 
transmitted in unsecured way. For this reason it is highly recommended 
to configure your Firewall to not expose the port used by NZBGet to WAN. 

If you need to control server from WAN it is better to connect to server's
terminal via SSH (POSIX) or remote desktop (Windows) and then run
nzbget-client-commands in this terminal.

### Web-interface
-----------------

NZBGet has a built-in web-server providing the access to the program
functions via web-interface.

To activate web-interface set the option "WebDir" to the path with
web-interface files. If you install using "make install-conf" as
described above the option is set automatically. If you install using
binary files you should check if the option is set correctly.

To access web-interface from your web-browser use the server address
and port defined in NZBGet configuration file in options "ControlIP" and
"ControlPort". For example:
```
http://localhost:6789/
```

For login credentials type username and the password defined by
options "ControlUsername" (default `"nzbget"`) and "ControlPassword"
(default `"tegbzn6789"`).

In a case your browser forget credentials, to prevent typing them each
time, there is a workaround - use URL in the form:
```
http://localhost:6789/username:password/
```
Please note, that in this case the password is saved in a bookmark or in
browser history in plain text and is easy to find by persons having
access to your computer. 
