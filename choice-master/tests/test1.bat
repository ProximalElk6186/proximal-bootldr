@ECHO OFF
CHOICE /C:ASDF "press a,s,d or f"
IF ERRORLEVEL 4 GOTO F
IF ERRORLEVEL 3 GOTO D
IF ERRORLEVEL 2 GOTO S
IF ERRORLEVEL 1 GOTO A
GOTO FAIL

:F
ECHO F
GOTO DOS

:D
ECHO D
GOTO DOS

:S
ECHO S
GOTO DOS

:A
ECHO A
GOTO DOS

:FAIL
ECHO Failed!

:DOS
