## Overview

These instructions are intended to get users (not developers) of Phlex to a working environment, starting from scratch.
This is not the only way in which this can be done, but this method has been verified on the primary supported OS (Alma Linux 9) and will be supported by the Phlex development team on that platform.

We assume that you do *not* have an installation of Spack already.
We only support Spack v1.0 and newer, so if you have an older version of Spack we recommend installation of a new one according to these instructions.
If you already have a new-enough version of Spack installed, you can skip to step 6.

### Installing Spack

Step 1: create a working directory for phlex use:

```bash
export PHLEX_WORK_DIR="/scratch/$(id -un)/phlex-work-dir
mkdir -p ${PHLEX_WORK_DIR}
cd ${PHLEX_WORK_DIR}
```

Step 2: remove any previous user-level Spack installation
```bash
rm -rf $HOME/.spack
```

Then make sure you don't already have a spack  command on your PATH:
```bash 
which spack
```
should tell you there is no spack in your PATH.

Step 3: install the fermilab version of Spack and related tools. 
```bash
wget https://raw.githubusercontent.com/FNALssi/fermi-spack-tools/refs/heads/fnal-v1.1.0/bin/bootstrap
bash bootstrap $PWD/spack-fnal
```
The bootstrap takes about 2 minutes.
The output from a successful bootstrap will end up in `Bootstrap completed properly, see logfile for details`

Step 4: make spack available at the command line.
```bash
source ${PHLEX_WORK_DIR}/spack-fnal/share/spack/setup-env.sh
```
`which spack` will show that `spack` is a bash function. 


Step 5: Modify the Spack configuration to avoid filling `/tmp`.
This is not at all related to Phlex, but until there is spack documentation describing this, we recommend it as good practice.

Run the following, which will open an editor.
```bash
spack config edit config
```
Modify the file to include a new key, `build_stage`.
`config` key will already be in the file and may already contain other keys. 

```yaml
config:
  build_stage:
      - $spack/var/spack/stage
```

### Ensuring a sufficient compiler

Step 6: ensure that spack has access to a new enough GCC.
Currently this means GCC 14. 

Run `spack compilers` to see what compilers Spack knows about.
If you don't have GCC 14 or newer, then install GCC 14:

```bash
spack install -j 12 gcc@14
```

Step 7: Add the Spack recipe repository for Phlex:

```bash
spack repo add https://github.com/Framework-R-D/phlex-spack-recipes.git
```

After you have done this, `spack repo list` should show that you have (among others) a repository named `phlex` available to Spack.

Step 8: Create a spack environment and install phlex

To guide the creation of the environment, download the environment configuration file, and create the environment it describes:

```bash
spack env create my-phlex-environment
spack env activate my-phlex-environment
spack add phlex %gcc@14
spack concretize
```

The concretization can take up to a few minutes.
If it is successful you will see the concretization results listed followed by a notice that spack is updating a few at some path.

You are then ready to build the Phlex environment:

```bash
spack install -j 12  # choose a suitable number of jobs for your machine
```

While the build of `phlex` itself takes a couple of minutes, this install may need to build (and not just download) other packages.
Thus the full installation may take an hour or so.

When the installation is complete, you should find that the `phlex` executable is on your PATH.
`which phlex` will verify this.

