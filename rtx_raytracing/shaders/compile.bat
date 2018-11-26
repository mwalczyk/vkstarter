:: rasterization
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/shader.vert -o shaders/vert.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/shader.frag -o shaders/frag.spv

:: primary rays
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/pri.rgen -o shaders/pri_rgen.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/pri.rchit -o shaders/pri_rchit.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/pri.rmiss -o shaders/pri_rmiss.spv

:: shadow rays
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/sec.rchit -o shaders/sec_rchit.spv
%VK_SDK_PATH%/Bin32/glslangValidator.exe -V shaders/sec.rmiss -o shaders/sec_rmiss.spv

pause