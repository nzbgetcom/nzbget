## About

Scheduler extensions are called by scheduler tasks (setup by the user).

## Writing scheduler extensions

> Please read [Extensions](EXTENSIONS.md) for general information about extensions first.

## Schedule

Users can activate scheduler extensions in two ways:

By creating a scheduler task with `TaskX.Command=Script` and choosing the extension. The schedule time is configured 
via scheduler task in option `TaskX.Time`.
For many scheduler extensions a reasonable default schedule can be provided by extension author, via extension definition. 
Scheduler extensions with default schedule can be activated/deactiated the Extension Manager; 
no creation of a scheduler task is required. If user doesn’t like the default scheduling he can create a scheduler task 
(instead of or in addition to activating the script via option "Extensions").

Example (explained below):
```json
{
  "main": "main.py",
  "name": "MyExtension",
  "homepage": "https://github.com/nzbgetcom/Extension-MyExtension",
  "kind": "QUEUE/SCHEDULER/POST-PROCESSING",
  "displayName": "My Extension",
  "version": "2.0",
  "author": "John Doe",
  "license": "GNU",
  "about": "Test script.",
  "queueEvents": "NZB_ADDED",
  "requirements": [],
  "description": ["This is a long description for test script."],
  "options": [
    {
      "name": "Option1",
      "displayName": "Option 1",
      "value": "yes",
      "description": ["Example script option (yes, no)."],
      "select": ["yes", "no"]
    }
  ],
  "commands": [],
  "taskTime": "*;*:00;*:30"
}
```
