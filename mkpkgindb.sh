#!/bin/sh

# $Id: mkpkgindb.sh,v 1.1.1.1 2011/03/03 14:43:12 imilh Exp $

echo "/* automatically generated, DO NOT EDIT */"
echo '#define CREATE_DRYDB " \'
${SEDCMD} -E -e 's/$/ \\/' -e 's/\"/\\\"/g' pkgin.sql
echo '"' 
