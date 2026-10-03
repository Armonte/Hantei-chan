#!/bin/bash
powershell.exe -NoProfile -Command "Stop-Process -Id $1 -Force" 2>&1 | tr -d '\r'
