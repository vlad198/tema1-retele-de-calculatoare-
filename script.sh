#!/bin/sh

[ -e "SEND_COMMAND_TO_CHILDREN" ] && rm  "SEND_COMMAND_TO_CHILDREN"
[ -e "flag.txt" ] && rm  "flag.txt"

gcc tema.c -o tema.bin

clear

./tema.bin