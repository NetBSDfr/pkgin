#!/bin/sh

echo "/* automatically generated, DO NOT EDIT */"
echo '#define CREATE_DRYDB " \'
${SEDCMD} -e 's/$/ \\/' -e 's/\"/\\\"/g' pkgin.sql
echo '"' 
