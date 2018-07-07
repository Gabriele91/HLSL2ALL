@echo off
REM dir path
@set TOP=%~dp0
@set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSbuild.exe"
REM go to top
@cd %TOP%
REM echo
@echo on
REM clone git repo
git clone https://github.com/KhronosGroup/SPIRV-Cross.git spriv_cross_tmp_x64
@echo off
REM path of source
@set SOURCE_DIR=%TOP%spriv_cross_tmp_x64\
@set BUILD_DIR=%SOURCE_DIR%buind\
@set INSTALL_DIR=%SOURCE_DIR%install\
REM cmake
@mkdir "%BUILD_DIR%"
@mkdir "%INSTALL_DIR%"
REM call cmake
@cd %SOURCE_DIR%
@echo on
cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" %SOURCE_DIR%
REM call build tools
%MSBUILD% ALL_BUILD.vcxproj /p:Configuration=Release /p:Platform=x64
@echo off
REM back
@cd "%TOP%"
REM include dir build
@mkdir "%TOP%dependencies\x64\include\spirv_cross\"
REM copy
@move /y "%SOURCE_DIR%Release\*.lib" "%TOP%dependencies\x64\lib"
@move /y "%SOURCE_DIR%*.h" "%TOP%dependencies\x64\include\spirv_cross\"
@move /y "%SOURCE_DIR%*.hpp" "%TOP%dependencies\x64\include\spirv_cross\"
REM delete
@del /s /q  "%SOURCE_DIR%*"
@rmdir  /s /q  "%SOURCE_DIR%"
REM echo
@echo on