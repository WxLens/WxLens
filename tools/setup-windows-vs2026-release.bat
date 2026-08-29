@set script_dir=%~dp0

@set build_dir=%script_dir%\..\build-release-vs2026
@set build_type=Release
@set conan_profile=wxlens-windows_vs2026_x64
@set generator=Visual Studio 18 2026
@set qt_base=C:/Qt
@set qt_arch=msvc2022_64
@set venv_path=

:: Assign user-specified build directory
@if not "%~1"=="" set build_dir=%~f1

:: Perform common setup
@call %script_dir%\lib\setup-common.bat
