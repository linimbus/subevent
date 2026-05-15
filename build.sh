#!/bin/bash

rm -f tests

g++ -std=c++17 -g test_SingleEvent.cpp test_MultiEvent.cpp -lgtest -lgtest_main -pthread -o tests

./tests