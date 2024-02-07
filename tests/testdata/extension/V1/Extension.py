### TASK TIME: *;*:00;*:30                           					   ###
##############################################################################
### NZBGET QUEUE/POST-PROCESSING SCRIPT                                    ###
#########################################################
### QUEUE EVENTS: NZB_ADDED, NZB_DOWNLOADED, FILE_DOWNLOADED		       ###

# About1.
# About2.
#
# Description1
# Description2
#
# NOTE: This script requires Python to be installed on your system.

##############################################################################
### OPTIONS    

# Append list of files to the message (yes, no).
#
# Add the list of downloaded files (the content of destination directory).
#FileList=yes

# SMTP server port (1-65535).
#Port=25

# When to send the message (Always, OnFailure).
#SendMail=Always

# Secure communication using TLS/SSL (yes, no, force).
#  no    - plain text communication (insecure);
#  yes   - switch to secure session using StartTLS command;
#  force - start secure session on encrypted socket.
#Encryption=yes

# To check connection parameters click the button.
#ConnectionTest@Send Test E-Mail

# Email address you want this email to be sent to.
#
# Multiple addresses can be separated with comma.
#To=myaccount@gmail.com

# Banned extensions.
#
# Extensions must be separated by a comma (eg: .wmv, .divx).
#BannedExtensions=

# Common specifiers (for movies, series and dated tv shows):
#
# {TEXT}          - lowercase the text.
#MoviesFormat=%t (%y)

# ffmpeg output settings.
#outputVideoExtension=.mp4
#outputVideoCodec=libx264

# Output Default (None, iPad, iPad-1080p, iPad-720p, Apple-TV2, iPod).
#
# description
#outputDefault=None

## General

# Auto Update nzbToMedia (0, 1).
#
# description
#auto_update=0

## Posix

# Niceness for external extraction process.
#
# Set the Niceness value for the nice command (Linux). These range from -20 (most favorable to the process) to 19 (least favorable to the process).
#niceness=10

# ionice scheduling class (0, 1, 2, 3).
#
# Set the ionice scheduling class (Linux). 0 for none, 1 for real time, 2 for best-effort, 3 for idle.
#ionice_class=2

# ionice scheduling class.
#
# Set the ionice scheduling class (0, 1, 2, 3).
#ionice_class=2

### NZBGET POST-PROCESSING SCRIPT                                          ###
##############################################################################
