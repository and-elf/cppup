@echo off

REM Environment setup script for test_build_project

REM Add .cppup\bin to PATH
set PATH=./test_build_project/.cppup\bin;%PATH%

REM Set project root
set CPPUP_PROJECT_ROOT=./test_build_project

echo Environment set up for test_build_project
echo Tools available: 
dir /b ./test_build_project/.cppup\bin 2>nul || echo none
