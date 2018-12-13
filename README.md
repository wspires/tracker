# tracker

## Overview
Class tracker is a factory that can access objects it has made.
Tracked objects are not owned by the tracker; callers own the objects made.
A tracked object that is deleted will automatically detach itself from its tracker.
A tracker is automatically notified when an object is attached or detached and can be customized at compile-time to handle these events.

This class and its tests demonstrate various features of modern c++ (c++11/14):
* Rvalue references and move constructors
* Uniform initialization
* Type inference with auto
* Range-based for loops
* Lambda functions and expressions
* Variadic templates
* Explicitly defaulted and deleted special member functions
* Smart pointers

## Files
* tracker.hpp - Implementation of tracker class
* tracker_test.cpp - Unit tests for tracker
* find.hpp - Helper for tracker container
* static_dispatch.hpp - Macro used by tracker
* Makefile - Compile and link unit tests
* run.sh - Make and run tests
* Catch2/ - Small, header-only unit test framework (not my code: https://github.com/catchorg/Catch2)

## Build and Run
Build and run at the command-line:
$ ./run.sh

This will call make using the included Makefile and run the tests. Equivalently:
$ g++ -c  -std=c++14 -g -Wall tracker_test.cpp  -ICatch2/single_include
$ g++  -std=c++14 -g -Wall -o tracker_test tracker_test.o    -lm   -ICatch2/single_include
$ tracker_test --success 

## Supported Environments
* Ubuntu 18.04
  g++ (Ubuntu 7.3.0-27ubuntu1~18.04) 7.3.0

* macOS 10.14
  Apple LLVM version 10.0.0 (clang-1000.11.45.5)
  Target: x86_64-apple-darwin18.2.0
