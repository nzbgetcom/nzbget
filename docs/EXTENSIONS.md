## About

The core functionality of NZBGet can be extended using extensions. 
NZBGet provides documented entry points for extensions. 
On certain events the extensions are executed, they receive information about event, 
do certain work and can communicate with NZBGet to give it instructions for further processing.

## Installation

### Extension Manager

On the `SETTINGS` page you can easily find `EXTENSION MANAGER` 
and use it to install the extensions you need. Make sure that the internet connection is working 
and the path to the `7z` in `UNPACK->SevenZipCmd`(required for unpacking the extensions) is correct.
The list of available extensions can be found in the [nzbget-extensions](https://github.com/nzbgetcom/nzbget-extensions) repository.

### Manual, for your custom extensions
Option `ScriptDir` defines the location of extensions. 
To make an extension available in NZBGet put the extension into this directory. 
Then go to settings tab in web-interface (if you were already on settings tab switch to downloads tab and then back to settings 
tab to reread the list of available scripts from the disk).

Menu at the left of page should list all extensions found in `ScriptDir`. 
Select a script to review or change its options (if it has any).

## Writing extension

In general, an extension consists of 2 files: `manifest.json` - with all the meta-data describing the extension and the `executable file`, like `main.py` or `main.exe`.

### Example

`manifest.json`
```json
{
    "main": "main.py",
    "name": "Test",
    "homepage": "https://github.com/nzbgetcom/Extension-Test",
    "kind": "POST-PROCESSING",
    "displayName": "My Test Extension",
    "version": "2.0.0",
    "author": "Author's name",
    "license": "GNU",
    "about": "Does something useful.",
    "queueEvents": "",
    "requirements": [
        "This script requires Python 3.8+ to be installed on your system."
    ],
    "description": [
        "Does something useful and ",
        "even more."
    ],
    "options": [
        {
            "name": "sayHiOrBye",
            "displayName": "Say \"Hi\" or \"Bye\"",
            "value": "Hi",
            "description": ["Say \"Hi\" or \"Bye\", and that's it."],
            "select": ["Hi", "Bye"]
        },
    ],
    "commands": [
        {
            "name": "ConnectionTest",
            "action": "Test",
            "displayName": "Connection Test",
            "description": ["To check connection parameters click the button."]
        }
    ],
    "taskTime": ""
}
```
