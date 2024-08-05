## About

The core functionality of NZBGet can be extended using extensions. 
NZBGet provides documented entry points for extensions. 
On certain events the extensions are executed, they receive information about event, 
do certain work and can communicate with NZBGet to give it instructions for further processing.

## Installation

### Using the Extension Manager

On the `SETTINGS` page you can easily find `EXTENSION MANAGER` 
and use it to install the extensions you need. Make sure that the internet connection is working 
and the path to the `7z` in `UNPACK->SevenZipCmd`(required for unpacking the extensions) is correct.
The list of available extensions can be found in the 
[nzbget-extensions](https://github.com/nzbgetcom/nzbget-extensions) repository.

### Manually, for your custom extensions

Option `ScriptDir` defines the location of extensions. 
To make an extension available in NZBGet put the extension into this directory. 
Then go to settings tab in web-interface (if you were already on settings 
tab switch to downloads tab and then back to settings 
tab to reread the list of available extensions from the disk).

Menu at the left of page should list all extensions found in `ScriptDir`. 
Select an extension to review or change its options (if it has any).

## Writing extension

In general, an extension consists of 2 files: `manifest.json` - 
with all the meta-data describing the extension 
and the `executable` file, like `main.py`.

`manifest.json` example:
```json
{
  "main": "main.py",
  "name": "EMail",
  "homepage": "https://github.com/nzbgetcom/Extension-EMail",
  "kind": "POST-PROCESSING",
  "displayName": "My EMail Extension",
  "version": "2.0",
  "nzbgetMinVersion": "24",
  "author": "John Doe",
  "license": "GNU",
  "about": "Sends E-Mail notification.",
  "queueEvents": "",
  "taskTime": "",
  "requirements": [
    "This script requires Python3.8 to be installed on your system."
  ],
  "description": ["This script sends E-Mail notification when the job is done."],
  "options": [
    {
      "name": "Server",
      "displayName": "Server",
      "value": "smtp.gmail.com",
      "description": ["SMTP server host."],
      "select": []
    },
    {
      "name": "Port",
      "displayName": "Port",
      "value": 25,
      "description": ["SMTP server port (1-65535)"],
      "select": [1, 65535]
    },
    {
      "section": "Categories",
      "name": "SendMail",
      "displayName": "SendMail",
      "value": "Always",
      "description": ["When to send the message."],
      "select": ["Always", "OnFailure"]
    }
  ],
  "commands": [
    {
      "name": "ConnectionTest",
      "action": "Send Test E-Mail",
      "displayName": "ConnectionTest",
      "description": ["To check connection parameters click the button."]
    },
    {
      "section": "Feeds",
      "name": "ConnectionTest",
      "action": "Send Test E-Mail",
      "displayName": "ConnectionTest",
      "description": ["To check connection parameters click the button."]
    }
  ],
  "sections": [
   {
      "name": "Categories",
      "prefix": "Category",
      "multi": true
    },
    {
      "name": "Feeds",
      "prefix": "Feed",
      "multi": false
    }
  ]
}
```

### `"main"`

The name of the executable file. 
For example: `main.py`, `main.exe`, `MyExt.sh`.

### `"name"`

The extension name, identifier and prefix needed to 
save the extension configuration options to `nzbget.conf`.

### `"homepage"`

Just a link to the repository where the extension lives.

### `"kind"`

Depending on the purpose of the extension, can be:
```
"kind": "POST-PROCESSING"
"kind": "SCAN"
"kind": "QUEUE"
"kind": "SCHEDULER"
"kind": "FEED"
```

If the extension can be used for multiple purposes the `kind` can be mixed, for example:
```
"kind": "SCAN/QUEUE"
```

More information about extension kinds can be found at:
 - [POST-PROCESSING](POST-PROCESSING.md)
 - [SCAN](SCAN.md)
 - [QUEUE](QUEUE.md)
 - [SCHEDULER](SCHEDULER.md)
 - [FEED](FEED.md)

### `"displayName"`

The name that will be displayed in the web interface.

### `"version"`

Extension version.

### `"nzbgetMinVersion" (optional)`

NZBGet minimum required version.

### `"author"`

Author's name.

### `"license"`

Extension license.

### `"about"`

Brief description of the extension.

### `"description"`

For more detailed description of the extension.

### `"queueEvents"`

To describe the events that could be used by the extension, e.g.:

```json
"queueEvents": "NZB_ADDED, NZB_DOWNLOADED"
```

More information can be found at [QUEUE](QUEUE.md).

### "`requirements"`

To describe the list of requirements, e.g.:

```json
"requirements": [
  "This script requires Python3.8+ to be installed on your system."
],
```

### `"options"`

Let’s say we are writing an extension to send E-Mail notification. 
The user needs to configure the extension with options such as SMTP-Server host, 
port, login and password. To avoid hard-coding of these data in the extension 
NZBGet allows the extension to define the required options. 
The options are then made available on the settings page for user to configure.

In the provided `manifest.json` example we defined three options:

```json
"options": [
  {
    "name": "Server",
    "displayName": "Server",
    "value": "smtp.gmail.com",
    "description": ["SMTP server host."],
    "select": []
  },
  {
    "section": "Categories",
    "name": "Port",
    "displayName": "Port",
    "value": 25,
    "description": ["SMTP server port (1-65535)"],
    "select": [1, 65535]
  },
  {
    "name": "SendMail",
    "displayName": "SendMail",
    "value": "Always",
    "description": ["When to send the message."],
    "select": ["Always", "OnFailure"]
  }
],
```
>`"section"` property is optional. Default value is `"options"`. 
> Required `"nzbgetMinVersion"`: `"24"`.

When the user saves settings in web-interface the extension configuration 
options are saved to NZBGet configuration file using the extension name as prefix. For example:
```
EMail:Server=smtp.gmail.com
EMail:Port=25
```

Extension configuration options are passed using env-vars with prefix `NZBPO_`. 
There are two env-vars for each option:
 - one env-var with the name exactly as defined by pp-option;
 - another env-var with the name written in UPPER CASE and with 
 special characters replaced with underscores.

For example, for pp-option `Server.Name` two env-vars are passed: 
`NZBPO_Server.Name` and `NZBPO_SERVER_NAME`.

> In a case the user has installed the extension but have not saved the configuration, the options are not saved to 
configuration file yet. The extension will not get the options passed. 
This is a situation your extension must handle. You can either use a default settings or terminate the extension with 
a proper message asking the user to check and save configuration in web-interface. 

Example (python):
```python
required_options = ('NZBPO_FROM', 'NZBPO_TO', 'NZBPO_SERVER', 'NZBPO_PORT', 'NZBPO_ENCRYPTION',
'NZBPO_USERNAME', 'NZBPO_PASSWORD', 'NZBPO_FILELIST', 'NZBPO_BROKENLOG', 'NZBPO_POSTPROCESSLOG')
for optname in required_options:
if (not optname in os.environ):
    print('[ERROR] Option %s is missing in configuration file. Please check script settings' % optname[6:])
    sys.exit(POSTPROCESS_ERROR)
```

### `"commands"`

Sometimes it may be helpful to be able to execute extension extensions from settings page. 
For example pp-extension EMail could use a button "Send test email". For other extensions something like 
"Validate settings" or "Cleanup database" may be useful too.

Starting from v19 it is possible to put buttons on the extension settings page. The buttons are defined as 
part of extension configuration, almost similar to extension configuration options:
```json
"commands": [
  {
    "section": "Categories",
    "name": "ConnectionTest",
    "action": "Send Test E-Mail",
    "displayName": "ConnectionTest",
    "description": ["To check connection parameters click the button."]
  }
],
```

This example creates a button with text "Send Test E-Mail" and description 
"To check connection parameters click the button.".

When user presses the button NZBGet executes the extension in a special context passing 
button name via env. var NZBCP_COMMAND. 
The extension can check if it runs in command mode by examining this variable (python example):
```python
# Exit codes used by NZBGet
COMMAND_SUCCESS = 93
COMMAND_ERROR = 94

# Check if the script is executed from settings page with a custom command
command = os.environ.get('NZBCP_COMMAND')
test_mode = command == 'ConnectionTest'
if command != None and not test_mode:
    print('[ERROR] Invalid command ' + command)
    sys.exit(COMMAND_ERROR)

if test_mode:
    print('[INFO] Test connection...')
    sys.exit(COMMAND_SUCCESS)

<script continues in normal mode>
```

During execution of the script NZBGet presents a special dialog showing extension output. 
The extension must exit with one of predefined exit codes indicating success 
(exit code `93`) or failure (exit code `94`).

User may close the progress dialog but the extension continues running in the background. 
All messages printed by the extension are saved to NZBGet log and are seen in web-interface on Messages tab.

### `"sections" (optional)`. 
>Required `"nzbgetMinVersion"`: `"24"`.

`Sections` are used to logically organize options and commands in `web-interface`.
>`Sections` with the reserved name `"options"` will be ignored.

```json
"sections": [
	{
		"name": "Categories",
		"prefix": "Category",
		"multi": true
	},
	{
		"name": "Feeds",
		"prefix": "Feed",
		"multi": false
	}
]

```

`"multi"` means that `options` or `commands` can be added dynamically in `web-intefrace.`

`"prefix"` is relevant for `multi` sections to avoid name conflicts of `options` saved in `nzbget.conf`.

Example of a `Name` option saved in `nzbget.conf` that refers to the `"Categories"` section with the prefix `"Category"`:

```conf
SpeedControl:Category1.Name=
SpeedControl:Category2.Name=
```

### `"taskTime"`

For [SCHEDULER](SCHEDULER.md) extensions.

Example:
```json
"taskTime": "*;*:00;*:30"
```

In that example the extension will be started at program boot (*) and then every 30 minutes.

## Tips

### Testing NZBGet version
NZBGet version is passed in env-var NZBOP_VERSION. Example values:

for stable version: nzbget-11.0;
for testing version: nzbget-11.0-testing-r620
Example (python):
```python
NZBGetVersion=os.environ['NZBOP_VERSION']
if NZBGetVersion[0:5] < '11.1':
    print('[ERROR] This script requires NZBGet 11.1 or newer. Please update NZBGet')
    sys.exit(POSTPROCESS_ERROR)
```

## Communication with NZBGet via RPC-API

With RPC-API more things can be done than using command line. For documentation on available 
RPC-methods see [API](https://nzbget.com/documentation/api).

Example: obtaining post-processing log of current nzb-file (this is a short version of script Logger.py supplied with NZBGet):

```python
import os
import sys
import datetime
from xmlrpclib import ServerProxy

## Exit codes used by NZBGet
POSTPROCESS_SUCCESS = 93
POSTPROCESS_ERROR = 94

# To get the post-processing log we connect to NZBGet via XML-RPC
# and call method "postqueue", which returns the list of post-processing job.
# The first item in the list is current job. This item has a field 'Log',
# containing an array of log-entries.
# For more info visit https://nzbget.com/documentation/api/

# First we need to know connection info: host, port, username and password of NZBGet server.
# NZBGet passes all configuration options to post-processing script as
# environment variables.
host = os.environ['NZBOP_CONTROLIP']
port = os.environ['NZBOP_CONTROLPORT']
username = os.environ['NZBOP_CONTROLUSERNAME']
password = os.environ['NZBOP_CONTROLPASSWORD']

if host ## '0.0.0.0': host = '127.0.0.1'

# Build an URL for XML-RPC requests
rpcUrl = 'http://%s:%s@%s:%s/xmlrpc' % (username, password, host, port)

# Create remote server object
server = ServerProxy(rpcUrl)

# Call remote method 'postqueue'. The only parameter tells how many log-entries to return as maximum.
postqueue = server.postqueue(10000)

# Get field 'Log' from the first post-processing job
log = postqueue[0]['Log']

# Now iterate through entries and save them to the output file
if len(log) > 0:
f = open('%s/_postprocesslog.txt' % os.environ['NZBPP_DIRECTORY'], 'w')
for entry in log:
    f.write('%s\t%s\t%s\n' % (entry['Kind'], datetime.datetime.fromtimestamp(int(entry['Time'])), entry['Text']))
    f.close()

sys.exit(POSTPROCESS_SUCCESS)
```
