# Windows Screensaver

Screensaver application for Windows.
(A screensaver is btw a relic used decades ago (simular to the Windows operating system))

### Compiling

The application was built using `MSBuild`. To compile the file, the `MSBuild toolchain` and the `Windows SDK` must be installed.

Both components can be installed via the `Visual Studio Installer` under:

- `MSVC v143 - VS 20xx C++ x64/x86 Build tools`
- `Windows 10 SDK`

Afterward, the project can be compiled and linked with the following command in the `Developer PowerShell for VS 20xx`:

```powershell
msbuild screensaver.vcxproj /p:Configuration=Release /p:Platform=x64
```