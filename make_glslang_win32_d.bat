@echo off
REM dir path
@set TOP=%~dp0
@set MSBUILD="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSbuild.exe"
REM go to top
@cd %TOP%
REM echo
@echo on
REM clone git repo
git clone https://github.com/KhronosGroup/glslang.git glslang_tmp_win32_d
@echo off
REM path of source
@set SOURCE_DIR=%TOP%glslang_tmp_win32_d\
@set BUILD_DIR=%SOURCE_DIR%buind\
@set INSTALL_DIR=%SOURCE_DIR%install\
REM cmake
@mkdir "%BUILD_DIR%"
@mkdir "%INSTALL_DIR%"
REM call cmake
@cd %SOURCE_DIR%
@echo on
cmake -G "Visual Studio 15 2017" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%" %SOURCE_DIR%
REM call build tools
%MSBUILD% ALL_BUILD.vcxproj /p:Configuration=Debug /p:Platform=Win32
@echo off
REM back
@cd "%TOP%"
REM include dir build
@mkdir "%TOP%dependencies\Win32\include\glslang\"
@mkdir "%TOP%dependencies\Win32\include\glslang\Include\"
@mkdir "%TOP%dependencies\Win32\include\glslang\MachineIndependent\"
@mkdir "%TOP%dependencies\Win32\include\glslang\MachineIndependent\preprocessor"
@mkdir "%TOP%dependencies\Win32\include\glslang\Public\"
@mkdir "%TOP%dependencies\Win32\include\SPIRV\"
REM copy
@move /y "%SOURCE_DIR%glslang\Include\*.h" "%TOP%dependencies\Win32\include\glslang\Include\"
@move /y "%SOURCE_DIR%glslang\MachineIndependent\*.h" "%TOP%dependencies\Win32\include\glslang\MachineIndependent\"
@move /y "%SOURCE_DIR%glslang\MachineIndependent\preprocessor\*.h" "%TOP%dependencies\Win32\include\glslang\MachineIndependent\preprocessor\"
@move /y "%SOURCE_DIR%glslang\Public\*.h" "%TOP%dependencies\Win32\include\glslang\Public\"
@move /y "%SOURCE_DIR%SPIRV\*.h" "%TOP%dependencies\Win32\include\SPIRV\"
@move /y "%SOURCE_DIR%SPIRV\*.hpp" "%TOP%dependencies\Win32\include\SPIRV\"
REM copy
@move /y "%SOURCE_DIR%glslang\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"
@move /y "%SOURCE_DIR%glslang\OSDependent\Windows\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"

@move /y "%SOURCE_DIR%SPIRV\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"
@move /y "%SOURCE_DIR%hlsl\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"
@move /y "%SOURCE_DIR%OGLCompilersDLL\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"
@move /y "%SOURCE_DIR%StandAlone\Debug\*.lib" "%TOP%dependencies\Win32\lib_d\"
REM delete
@del /s /q  "%SOURCE_DIR%*"
@rmdir  /s /q  "%SOURCE_DIR%"
REM echo
@echo on