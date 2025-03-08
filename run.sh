#!/bin/bash

clear
clear
rm main.exe
gcc main.c -o main
javac -encoding utf-8 LTNServer.java
java LTNServer
