#!/bin/bash

rm -rf build && mkdir build && cd build && cmake -DCODE_COVERAGE=ON .. && cmake --build . --config Debug --target coverage

