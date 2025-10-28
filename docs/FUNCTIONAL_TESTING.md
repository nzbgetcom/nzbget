**Note: The current implementation of functional tests is broken and requires updating.**

## Functional testing

During functional testing the program is tested as a whole. This is much like if the testing were made by a human. Although the user interface parts are not tested the core of the program is tested and how the different program components work together. When functional tests succeed that means that the tested scenarios will most likely work properly when the program is run by real users. The quality of total testing obviously depends on the amount and quality of individual tests.

**Note:** In addition to functional tests there are **unit tests**, which are not covered by this document.

## Infrastructure

The functional testing in NZBGet is controlled by scripts, which do things like:

- automatically start nzbget binary with a specific configuration;
- communicate with nzbget using nzbget API to control the program;
- create test data files and nzb-files referring to them;
- automatically start a special NNTP server, which serves test data files;
- tell nzbget to download test nzb-files from the test NNTP server;
- control the status of nzbget during download;
- check if the test downloads go as expected (success, failure);
- collect the results of all tests and represent the result summary.

## NServ - built-in NNTP server

  
NZBGet downloads from Usenet servers. However it would be unreliable and slow to use global Usenet servers for functional testing. Instead we need to do all the testing locally using our set of test files. For this purpose a simple NNTP server called **NServ** was developed.

Technically NServ is integrated into NZBGet as a sub-module. NServ is automatically controlled (started, stopped) by functional testing scripts but it can also be used standalone for other purposes such as debugging NZBGet or for speed tests of NZBGet or even other download clients. For more info see [NServ NNTP server](NSERV.md).

## Testing framework
  
Functional tests for NZBGet are written in python language. We are using testing framework [py.test](https://pytest.org) to organize test execution and collect test results. Therefore in order to run functional tests py.test [must be installed](https://docs.pytest.org/en/latest/getting-started.html) on your system.
  
## Running tests

Functional tests and supporting modules are stored in directory `tests/functional`. To run tests open command prompt in this directory and execute `py.test` as explained below.

### Configuring tests
  
Tests can be run without extra configuration in which case the test files are stored in `tests/testdata/nserv.temp` and temporary files created by NZBGet during testing are stored in `tests/testdata/nzbget.temp`. Both of these directories take several gigabytes of disk space and can be reconfigured to use other locations.

Create text file `tests/functional/pytest.ini` with the following content:

```bash
  [pytest]
  nzbget_bin=/path/to/nzbget
  sevenzip_bin=/path/to/7za
  nserv_datadir=/path/to/nserv.temp
  nzbget_maindir=/path/to/nzbget.temp
```
  
All settings in the ini-file are optional.

### Preparing test files

When executing the tests for the first time the test scripts prepare test files and put them into directory `tests/testdata/nserv.temp`. These files take several gigabytes and may need several minutes to generate. When starting tests for the first time it’s recommended to add parameter `-s` to see additional progress logging during preparation stage:

```bash
  py.test -v -s
```

### Executing tests

To run all tests use simple command:

```bash
  py.test -v
```

To run tests from one test script only pass the specific file name:

```bash
  py.test -v parcheck\parcheck_force_test.py
```

To execute only one specific test pass the file name and test function name:

```bash
  py.test -v parcheck/parcheck_auto_test.py::test_parchecker_repair
```
  
For more filter possibilities please see [py.test](https://docs.pytest.org/en/latest/usage.html) documentation.

### Test failures
  
Directory used by NZBGet during testing (`tests/testdata/nzbget.temp` by default) is automatically deleted if all tests succeed. If a test fails the directory is kept in order to preserve log-files and queue-files for failure analysis. However during testing NZBGet is started and stopped multiple times. If the failed test wasn’t the last test the preserved NZBGet directory may not contain data of the failed test. In such case it’s better to rerun the specific failed test as the only test (using specific command line).

After test completion NZBGet is terminated. To analyze a test failure it can be useful to have NZBGet running, for example to inspect the state in web-interface. Pass extra parameter `--hold` to achieve this:
