Step 1: create a working directory for phlex development:
```bash
export PHLEX_WORK_DIR="/scratch/$(id -un)/phlex-base"
mkdir -p ${PHLEX_WORK_DIR}
cd ${PHLEX_WORK_DIR}
```

Step 2: remove any previous user-level Spack installation
```bash
rm -rf $HOME/spack
```

Then make sure you don't already have a spack  command on your PATH:
```bash 
which spack
```
should tell you there is no spack in your PATH.

Step 3: install the fermilab version of Spack and related tools. 
```bash
wget https://raw.githubusercontent.com/FNALssi/fermi-spack-tools/refs/heads/fnal-v1.0.0/bin/bootstrap
bash bootstrap $PWD/spack-fnal
```
The bootstrap takes about 2 minutes.
The output from a successful bootstrap will end up in `Bootstrap completed properly, see logfile for details`

Step 4: make spack available at the command line.
```bash
source ${PHLEX_WORK_DIR}/spack-fnal/share/spack/setup-env.sh
```
`which spack` will show that `spack` is a bash function. 
`spack mpd --help` will show a help message if the installation is successful.

Step 5: initialize spack mpd.
```bash
spack mpd init
```
this step needs to be done once for one installation of spack. 

Step 6: run the following, which will open an editor.
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

Step 7: ensure that spack has access to a new enough GCC, currently this means GCC 14. 
run `spack compilers` and see what it reports. 
If you don't have GCC 14 or newer then rn `spack -j 12 gcc@14` 
