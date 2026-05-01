# Overview of release-test-script-ut-control.sh

This Bash script automates the process of cloning PR branch on ut-control Git repository, compiling code for different environments, and running checks to ensure the setup is correct. It supports environments such as **Ubuntu**, **Dunfell Linux**, **Dunfell ARM**, **VM-SYNC**, and **Cross** (any host toolchain).
where:
   Dunfell Linux : is a docker with linux environment
   Dunfell ARM : is a docker simulating the arm environment for yocto version dunfell
   Kirkstone ARM : is a docker simulating the arm environment for yocto version kirkstone
   VM-SYNC : is a docker simulating the RDK linux environment
   Cross : native cross-compilation on the host using any sourced OE/Yocto toolchain (e.g. aarch64, armv7, etc.)
With these validations, it ensures if the PR is good for merge and has not broken the basic requirements.

Following table gives an overview:

|#|Target|Docker|Expectation
|---|-----|-----------|---------
|1|make TARGET=arm|rdk-dunfell|builds ut-control for target arm
|2|make TARGET=arm|rdk-kirkstone|builds ut-control for target arm
|3|make TARGET=linux|vm-sync|builds ut-control for target linux
|4|make TARGET=linux|none|builds ut-control for target linux
|5|make TARGET=arm|none (host toolchain)|builds ut-control for the architecture defined by the sourced cross toolchain
|6|make -C tests/ TARGET=arm|rdk-dunfell|builds ut-control tests for target arm
|7|make -C tests/  TARGET=arm|rdk-kirkstone|builds ut-control tests for target arm
|8|make -C tests/ TARGET=linux|vm-sync|builds ut-control tests for target linux
|9|make -C tests/ TARGET=linux|none|builds ut-control tests for target linux
|10|make -C tests/ TARGET=arm|none (host toolchain)|builds ut-control tests for the architecture defined by the sourced cross toolchain

## Key Features

### 1. **Flexible Repository Management**
- The script allows specifying a Git repository URL and branch name via command-line arguments:
-  `-t <BRANCH_NAME>`: Specifies the branch name to clone.
  - `-u <REPO_URL>`: Specifies the Git repository URL but not mandatory
  - `-T <TOOLCHAIN_PATH>`: Path to the cross-compilation toolchain env-setup script (optional)
- If no URL is provided, it defaults to a repository URL (`git@github.com:rdkcentral/ut-control.git`).
- If `-T` is not provided, the script will interactively prompt the user for the toolchain path at startup. Pressing Enter without providing a path will skip all cross-compilation tests, and the user will be informed. The script will never fall back to a hardcoded default path.
- If a path is provided (via `-T` or the prompt) but the file does not exist at that location, the script will print a warning and skip cross-compilation tests without aborting execution of other environment tests.

### 2. **Automated Environment Setup**
- Handles environment-specific setups for various packages:

|#|ENV/PACKAGES|CMAKE(Host)|CURL(Target)|OPENSSL(Target)
|----|----|--------|------|---------|
|1|Ubuntu+build essentials|NO|YES|NO
|2|VM-SYNC|YES|YES|YES
|3|RDK-DUNFELL(arm)|NO|YES|YES
|4|RDK-DUNFELL(linux)|NO|YES|NO
|5|Cross (host toolchain)|NO|YES|YES

For ex:
On env 1,  the script will check for availability of CURL library, OpenSSL libraraies for Target.It would however would not look for CMAKE binary for host as this environment already provides cmake support which is provided by build essentials.
On env 2, on the other hand, none of the packages are present , hence the script will check for all of the packages listed above.

- The script creates directories based on the environment and clones the respective repository branch into those directories.

### 3. **Compilation and Logging**
- Compiles the project using `make`:
  - **Architecture-specific compilation** (`linux`, `arm`) based on the environment.
  - For cross builds, the toolchain env-setup script is sourced in a subshell before invoking `make`, keeping the host environment clean. The actual target architecture (e.g. aarch64, armv7) is determined from the `CC` variable set by the toolchain.
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
# Prompt for cross toolchain path at startup (press Enter to skip cross compilation)
./release-test-script-ut-control.sh -t <UT_CONTROL_BRANCH_NAME_TO_BE_TESTED>

# With an explicit cross toolchain env-setup script (any OE/Yocto toolchain)
./release-test-script-ut-control.sh -t <UT_CONTROL_BRANCH_NAME_TO_BE_TESTED> -T /path/to/environment-setup-<arch>-rdk-linux
```

## Caveats
During compilation, it has been observed that curl library doesn't get build when triggered from configure.sh of ut-control.However, when build as part of ut-core builds w/o any issue. Work around for this issue is to build the library manually w/o using script.
