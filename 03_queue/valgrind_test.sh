#!/bin/bash

make debug
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./bin/stack_test
make clean
