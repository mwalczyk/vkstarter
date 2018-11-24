:: rasterization
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shader.vert
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shader.frag

:: primary rays
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V pri.rgen -o pri_rgen.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V pri.rchit -o pri_rchit.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V pri.rmiss -o pri_rmiss.spv

:: shadow rays
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V sec.rchit -o sec_rchit.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V sec.rmiss -o sec_rmiss.spv

pause