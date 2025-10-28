#!/bin/bash
. /entrypoint.sh
exec ctest "$@"
