@echo off
rem Quick start: checks the tools, copies your CD, lifts, builds, and leaves a
rem shortcut here. See README "Getting Started". Safe to run again.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup.ps1" %*
