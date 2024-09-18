@ECHO off
chcp 65001
cls

REM Variables used by the "code":

SET /A isSafeModeNetworking = 0
SET /A isSafeModeCMD = 0

ECHO  /================\
ECHO  lPROXIMAL-BOOTLDRl
ECHO  \================/
ECHO  SELECT AN OS TO BOOT:
REM TODO: Add smtn for selecting an os and booting it, maybe launching a .bat file or an .exe file and doing smtn with it
PAUSE
cls
ECHO  /================\
ECHO  lPROXIMAL-BOOTLDRl
ECHO  l================l
ECHO /OS BOOTING OPTIONS\
ECHO \==================/
ECHO 1. NORMAL BOOT
ECHO 
ECHO 2. OS RECOVERY
ECHO 
ECHO 3. SAFE MODE:
ECHO 4. [] WITH NETWORKING?
ECHO 5. [] WITH COMMAND PROMT?

PAUSE
