command: shellcheck ci/entrypoint.sh
exit: 1

In ci/entrypoint.sh line 47:
. /spack/share/spack/setup-env.sh
  ^-----------------------------^ SC1091 (info): Not following: /spack/share/spack/setup-env.sh was not specified as input (see shellcheck -x).

For more information:
  <https://www.shellcheck.net/wiki/SC1091> -- Not following: /spack/share/spack...
