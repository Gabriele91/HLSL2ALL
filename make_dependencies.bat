@set TOP=%~dp0
cd %TOP%
call make_dirs.bat
call make_glslang_x64.bat
call make_spirv_cross_x64.bat
call make_glslang_win32.bat
call make_spirv_cross_win32.bat