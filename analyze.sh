#!/bin/bash

if [ $# -lt 2 ]; then
    echo "Usage: $0 <command> [args] <duration>"
    exit 1
fi

/usr/local/bin/code_analyzer "$@"
