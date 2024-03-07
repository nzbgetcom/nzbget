# How to Contribute #

While we are working on NZBGet project all the time, we still would really like and need help on many areas:

- Major - code contributions on features and bugs
- Improvements in tests 
- Improvements on documentation

We entice our users to participate in the project, please don't hesitate to get involved - create a [new issue](https://github.com/nzbgetcom/nzbget/issues/new) or [pull request](https://github.com/nzbgetcom/nzbget/compare) - that would also greatly help if you'll share your usage experience.

## Documentation ##

Main documentation is available on the NZBGet.com website - [https://nzbget.com/documentation/](https://nzbget.com/documentation/)

## Development ##

NZBGet natively supports for multiple platforms and build options, so each platform has their own development documenation, including:

- [General documentation, including Posix](https://github.com/nzbgetcom/nzbget/blob/develop/INSTALLATION.md)
- [Windows](https://github.com/nzbgetcom/nzbget/blob/develop/windows/README-WINDOWS.txt)
- [Docker](https://github.com/nzbgetcom/nzbget/blob/develop/docker/README.md)

### Branches naming policy ###

- `main` is a protected branch that contains only release code
- `develop` is a protected branch for development
- new branches should follow the following convention:
  - `hotfix/brief-description` for any small hotfixes
  - `feature/brief-description` for any new developments
  - `bugfix/brief-description` for bugs

### Pull requests flow for `develop` and `main` branches ### 

1. For PRs targeting `develop` branch `Squash and merge` mode must be used.
2. After merging branch to `develop`, branch must be deleted.
3. For release PR (`develop` -> `main`) `Create a merge commit` mode must be used.
4. After merging `develop` -> `main`, must be back merge `main` -> `develop` before any changes in `develop` branch.

This flow results to the fact that in the PR to master branch we see only the squashed commits that correspond to the PRs in the develop branch in current release cycle.

### Version changes in release cycle

After the release has been published (from the `main` branch), the minor version in the `develop` branch should be increased so that subsequent test builds are a higher version than the release.

List of files to change version:

1. configure.ac - "AC_INIT" macro
2. CMakeLists.txt - "project" block
3. nzbget.vcxproj - "PreprocessorDefinitions" blocks - 4 matches
4. windows/nzbget-setup.nsi - WriteRegStr - "DisplayVersion"
