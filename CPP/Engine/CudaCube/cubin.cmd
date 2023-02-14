@cls

@set VCInstallDir=%VS140COMNTOOLS%\..\..\VC\
rem @set VCInstallDir=%VS120COMNTOOLS%\..\..\VC\
@set CopyToBaseDir=%ITS%
@set CubinFileName=CubeKernel.cubin
@set ConfigurationName=Release
                         
@echo VCInstallDir = %VCInstallDir%
@echo CopyToBaseDir = %CopyToBaseDir%
@echo.

rem --preprocess 
@echo Compiling kernel 3.5
@nvcc.exe -ccbin "%VCInstallDir%bin" -cubin -gencode arch=compute_35,code=sm_35 -DNO_CUBE_ASSERTS -DWIN32 -D_CONSOLE -D_MBCS -Xcompiler /EHsc,/W3,/nologo,/O2,/Zi,/MT -I"%CUDA_INC_PATH%" -I./ -I../BasicCube -I../../Common -o .\%CubinFileName%.arch_35 CubeKernel.cu
rem @nvcc.exe -ccbin "%VCInstallDir%bin" -cubin -gencode arch=compute_35,code=sm_35 --machine 64 --use_fast_math -DNO_CUBE_ASSERTS -DWIN32 -D_CONSOLE -D_MBCS -Xcompiler /EHsc,/W3,/nologo,/O2,/Zi,/MT -I"%CUDA_INC_PATH%" -I./ -I../BasicCube -I../../Common -o .\%CubinFileName%.arch_35 CubeKernel.cu

@echo Compiling kernel 5.0
@nvcc.exe -ccbin "%VCInstallDir%bin" -cubin -gencode arch=compute_50,code=sm_50 -DNO_CUBE_ASSERTS -DWIN32 -D_CONSOLE -D_MBCS -Xcompiler /EHsc,/W3,/nologo,/O2,/Zi,/MT -I"%CUDA_INC_PATH%" -I./ -I../BasicCube -I../../Common -o .\%CubinFileName%.arch_50 CubeKernel.cu
rem @nvcc.exe -ccbin "%VCInstallDir%bin" -cubin -gencode arch=compute_50,code=sm_50 --machine 64 --use_fast_math -DNO_CUBE_ASSERTS -DWIN32 -D_CONSOLE -D_MBCS -Xcompiler /EHsc,/W3,/nologo,/O2,/Zi,/MT -I"%CUDA_INC_PATH%" -I./ -I../BasicCube -I../../Common -o .\%CubinFileName%.arch_50 CubeKernel.cu

@if %errorlevel% == 0 goto copykernel
@goto errorMessage

                                                                                                                                                                                                                   
:copykernel

@copy %CubinFileName%.*  %CopyToBaseDir%\Solutions\Debug\
@copy %CubinFileName%.*  %CopyToBaseDir%\Solutions\Release\
@copy %CubinFileName%.*  %CopyToBaseDir%\Tests\MaiTrixTest\
@copy %CubinFileName%.*  %CopyToBaseDir%\CPP\Face\Guide\

@goto end

:errorMessage

@echo ********* Compilation failed with error %errorlevel% ********* 
@exit 

:end

@echo. 
@echo ********* Compilation successful ********* 
@echo.