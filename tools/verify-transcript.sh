#!/bin/bash
#
# Uses the official Caia referee to verify the result of a game
#
# Usage:
#
#  verify-transcript.sh < transcript.txt
#

REFEREE=${REFEREE:-$HOME/caia/zeropointone/bin/referee}

if [ ! -x "$REFEREE" ]; then
    echo "Not an executable: $REFEREE"
    exit 1
fi

while read line; do
    echo lock_ok
    echo "$line"
done | "$REFEREE"
