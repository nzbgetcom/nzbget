# Install NZBGet for WIndows via package managers

- [Winget](#winget)
- [Chocolatey](#chocolatey)

- [Post-Install](#post-install)

## Winget

1. Make sure you have `winget` installed. Modern Windows versions have winget already pre-installed. Winget [documentation](https://learn.microsoft.com/windows/package-manager/winget/).
2. To install nzbget, run from a Command Prompt:
```
winget install nzbget
```

## Chocolatey

1. Install `chocolatey` package manager. Installation instructions can be found [here](https://docs.chocolatey.org/en-us/choco/setup/).
2. To install nzbget, run from an elevated Command Prompt:
```
choco install nzbgetcom
```

## Post-Install

1. Start NZBGet via start menu or desktop shortcut
2. NZBGet puts an icon into the tray area (near clock) and opens a browser window
3. In the web-interface (browser window) go to settings page:
    - Setup your login credentials in section SECURITY
    - Add one or more news-servers in section NEWS-SERVERS
    - Save settings and reload NZBGet
