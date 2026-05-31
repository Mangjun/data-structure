#!/bin/bash

make debug
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/list_test
make clean
