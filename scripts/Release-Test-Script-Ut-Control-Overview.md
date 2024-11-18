# Overview of release-test-script-ut-control.sh

This Bash script automates the process of cloning PR branch on ut-control Git repository, compiling code for different environments, and running checks to ensure the setup is correct. It supports environments such as **Ubuntu**, **Dunfell Linux**, **Dunfell ARM**, and **VM-SYNC**.
where:
   Dunfell Linux : is a docker with linux environment
   Dunfell ARM : is a docker simulating the arm environment for yocto version dunfell
   Kirkstone ARM : is a docker simulating the arm environment for yocto version kirkstone
   VM-SYNC : is a docker simulating the RDK linux environment
With these validations, it ensures if the PR is good for merge and has not broken the basic requirements.

Following table gives an overview:

|#|Target|Docker|Expectation
|---|-----|-----------|---------
|1|make TARGET=arm|rdk-dunfell|builds ut-control for target arm
|2|make TARGET=arm|rdk-kirkstone|builds ut-control for target arm
|3|make TARGET=linux|vm-sync|builds ut-control for target linux
|4|make TARGET=linux|none|builds ut-control for target linux
|5|make -C tests/ TARGET=arm|rdk-dunfell|builds ut-control tests for target arm
|6|make -C tests/  TARGET=arm|rdk-kirkstone|builds ut-control tests for target arm
|7|make -C tests/ TARGET=linux|vm-sync|builds ut-control tests for target linux
|8|make -C tests/ TARGET=linux|none|builds ut-control tests for target linux

## Key Features

### 1. **Flexible Repository Management**
- The script allows specifying a Git repository URL and branch name via command-line arguments:
-  `-t <BRANCH_NAME>`: Specifies the branch name to clone.
  - `-u <REPO_URL>`: Specifies the Git repository URL but not mandatory
- If no URL is provided, it defaults to a repository URL (`git@github.com:rdkcentral/ut-control.git`).

### 2. **Automated Environment Setup**
- Handles environment-specific setups for various packages:

|#|ENV/PACKAGES|CMAKE(Host)|CURL(Target)|OPENSSL(Target)
|----|----|--------|------|---------|
|1|Ubuntu+build essentials|NO|YES|NO
|2|VM-SYNC|YES|YES|YES
|3|RDK-DUNFELL(arm)|NO|YES|YES
|4|RDK-DUNFELL(linux)|NO|YES|NO

For ex:
On env 1,  the script will check for availability of CURL library, OpenSSL libraraies for Target.It would however would not look for CMAKE binary for host as this environment already provides cmake support which is provided by build essentials.
On env 2, on the other hand, none of the packages are present , hence the script will check for all of the packages listed above.

- The script creates directories based on the environment and clones the respective repository branch into those directories.

### 3. **Compilation and Logging**
- Compiles the project using `make`:
  - **Architecture-specific compilation** (`linux` or `arm`) based on the environment.
  - Logs the output of `make` to `make_log.txt` and `make_test_log.txt` for debugging purposes.
  
### 4. **Post-Build Verification**
- After compiling, the script verifies:
  - The current branch matches the target branch.
  - The existence of specific libraries such as:
    - **CURL static library** (`libcurl.a`)
    - **OpenSSL static library** (`libssl.a`)
    - **CMake binary**
- Each check outputs either `PASS` or `FAIL`, depending on whether the conditions are met.
- The checks vary based on the environment, allowing for customized validation for each setup.

### 5. **Parallel Execution Support**
- The script is capable of running builds and checks in parallel across multiple environments.
- Parallel execution commands are currently commented out but can be activated.

## Example Workflow
1. Clone the repository from the provided URL and branch.
2. Set up the environment-specific directory.
3. Compile the project for the appropriate architecture (e.g., Linux, ARM).
4. Perform checks to ensure the correct branch, libraries, and binaries are in place.
5. Output the results of each step.

## Command-Line Usage
```bash
./release-test-script-ut-control.sh -t <UT_CONTROL_BRANCH_NAME_TO_BE_TESTED>
```

## Caveats
During compilation, it has been observed that curl library doesn't get build when triggered from configure.sh of ut-control.However, when build as part of ut-core builds w/o any issue. Work around for this issue is to build the library manually w/o using script.