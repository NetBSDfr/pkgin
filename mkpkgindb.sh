#!/bin/sh

# $Id: mkpkgindb.sh,v 1.2 2011/09/11 08:41:20 imilh Exp $

echo "/* automatically generated, DO NOT EDIT */"
echo '#define CREATE_DRYDB " \'
${SEDCMD} -e 's/$/ \\/' -e 's/\"/\\\"/g' pkgin.sql
echo '"' 
