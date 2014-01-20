@echo off
setlocal
set CYGWIN_HOME=C:\cygwin64
if not exist CYGWIN_HOME (set CYGWIN_HOME=C:\cygwin)
copy "%CYGWIN_HOME%\bin\cygwin1.dll" cgi-bin
copy "%CYGWIN_HOME%\bin\cyggcc_s-1.dll" cgi-bin
copy "%CYGWIN_HOME%\bin\cyggcc_s-seh-1.dll" cgi-bin
copy "%CYGWIN_HOME%\bin\cygstdc++-6.dll" cgi-bin
