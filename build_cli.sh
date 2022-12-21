#!/bin/bash

clang -o cms50f_import CMS50F_Cli/main.c CMS50F/*.c -I CMS50F -Wall -Wpedantic -Werror -Wno-unused-function
clang -o cms50f_import_debug CMS50F_Cli/main.c CMS50F/*.c -I CMS50F -g -DDEBUG -Wall -Wpedantic -Werror