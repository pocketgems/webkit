# Building for Apportable
## Setup Environment
* Clone this repository and dragons3d
* Set environment variables
    * `ANDROID_NDK` Path to NDK (Either `~/Android/Sdk/ndk/version` or `~/Library/Android/sdk/ndk/version`)
        * Note `~` was used to denote your home directory; it is highly recommended that you use the full absolute path for these environment variables
    * `MANTIS_TOOLCHAIN_DIR` Path to mantis toolchain (`cocos3d/mantis-toolchain`)
    * `APPORTABLE_DIR` Path to apportable (`dragons3d/Externals/apportable`)

## Building
* Run `./do_make.sh {path to dragons}/projects/Tools/.static_apportable_libs`
* This will update the static libraries in .static_apportable_libs
* Commit updated libraries

## Updating
* Add upstream origin `git remote add upstream git@github.com:WebKit/WebKit-http.git`
* Fetch upstream master `git fetch upstream master`
* Rebase on top of upstream master `git rebase upstream/master`
* Follow build instructions

## Troubleshooting
### Symbol uploading fails
We may want to disable this, the symbols aren't very helpful. Comment out `./do_symbol_upload.py` in `do_make.sh` to avoid uploading.

### After rebasing, the build fails
See what the build failure is. The command line options of WebKit's scripts may change. Modify do_make.sh to reflect those changes.
