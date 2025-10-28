#!/bin/bash
. /entrypoint.sh
exec cmake "$@"
