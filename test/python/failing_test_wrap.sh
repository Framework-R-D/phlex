#!/bin/bash
"$@"
exit_code=$?
if [ $exit_code -ne 0 ]; then
    exit 1
fi
exit 0
