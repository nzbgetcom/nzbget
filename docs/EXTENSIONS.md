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
and the `executable file`, like `main.py`.

`manifest.json` example:
```json
{
  "main": "main.py",
  "name": "EMail",
  "homepage": "https://github.com/nzbgetcom/Extension-EMail",
  "kind": "POST-PROCESSING",
  "displayName": "My EMail Extension",
  "version": "2.0.0",
  "author": "John Doe",
  "license": "GNU",
  "about": "Sends E-Mail notification.",
  "queueEvents": "",
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
    }
  ],
  "commands": [
    {
      "name": "ConnectionTest",
      "action": "Send Test E-Mail",
      "displayName": "ConnectionTest",
      "description": ["To check connection parameters click the button."]
    }
  ],
  "taskTime": ""
}
```

### `main`

The name of the executable file. 
For example: `main.py`, `main.exe`, `MyExt.sh`.

### `name`

The extension name, identifier and prefix needed to 
save the extension configuration options to `nzbget.conf`.

### `homepage`

Just a link to the repository where the extension lives.

### `kind`

Can be:
```
"kind": "POST-PROCESSING"
"kind": "SCAN"
"kind": "QUEUE"
"kind": "SCHEDULER"
```
depending on the purpose of the extension.

If the extension can be used for multiple purposes the `kind` can be mixed, for example:
```
"kind": "SCAN/QUEUE"
```
More information about extension kinds can be found at [nzbget.com](https://nzbget.com/):

 - [POST-PROCESSING](https://nzbget.com/documentation/post-processing-scripts/)
 - [SCAN](https://nzbget.com/documentation/scan-scripts//)
 - [QUEUE](https://nzbget.com/documentation/queue-scripts/)
 - [SCHEDULER](https://nzbget.com/documentation/scheduler-scripts/)

### `displayName`

The name that will be displayed in the web interface.

### `version`

Extension version.

### `author`

Author's name.

### `license`

Extension license.

### `about`

Brief description of the extension.

### `description`

For more detailed description of the extension.

### `queueEvents`

To describe the events that could be used by the extension, e.g.:

```json
"queueEvents": "NZB_ADDED, NZB_DOWNLOADED"
```

More information can be found at [QUEUE](https://nzbget.com/documentation/queue-scripts/).

### `requirements`

To describe the list of requirements, e.g.:

```json
"requirements": [
  "This script requires Python3.8+ to be installed on your system."
],
```

### `options`

Let’s say we are writing an extension to send E-Mail notification. 
The user needs to configure the extension with options such as SMTP-Server host, 
port, login and password. To avoid hard-coding of these data in the extension 
NZBGet allows the extension to define the required options. 
The options are then made available on the settings page for user to configure.

In the provided `manifest.json` example we defined two options:

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
    "name": "Port",
    "displayName": "Port",
    "value": 25,
    "description": ["SMTP server port (1-65535)"],
    "select": [1, 65535]
  }
],
```

When the user saves settings in web-interface the extension configuration 
options are saved to NZBGet configuration file using the extension name as prefix. For example:
```
EMail:Server=smtp.gmail.com
EMail:Port=25
```

### `commands`

Sometimes it may be helpful to be able to execute extension scripts from settings page. For example pp-script EMail could use a button “Send test email”. For other extensions something like “Validate settings” or “Cleanup database” may be useful too.

Starting from v19 it is possible to put buttons on the script settings page. The buttons are defined as part of script configuration, almost similar to extension configuration options:

```json
"commands": [
  {
    "name": "ConnectionTest",
    "action": "Send Test E-Mail",
    "displayName": "ConnectionTest",
    "description": ["To check connection parameters click the button."]
  }
],
```

This example creates a button with text "Send Test E-Mail" and description 
"To check connection parameters click the button.".


### `taskTime`

For [SCHEDULER](https://nzbget.com/documentation/scheduler-scripts/) extensions.

Example:
```json
"taskTime": "*;*:00;*:30"
```

In that example the extension will be started at program boot (*) and then every 30 minutes.