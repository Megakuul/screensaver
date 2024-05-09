# Windows Screensaver
---

Screensaver application for Windows.
(A screensaver is btw a relic used decades ago (simular to the Windows operating system))

### Compiling
---

The application was built using `MSBuild`. To compile the file, the `MSBuild toolchain` and the `Windows SDK` must be installed.

Both components can be installed via the `Visual Studio Installer` under:

- `MSVC v143 - VS 20xx C++ x64/x86 Build tools`
- `Windows 10 SDK`

Afterward, the project can be compiled and linked with the following command in the `Developer PowerShell for VS 20xx`:

```powershell
msbuild screensaver.vcxproj /p:Configuration=Release /p:Platform=x64
```



### Usage
---

The screensaver can be installed via `Install` context button on the `x64/Release/screensaver.scr` file in the windows explorer.



### Customize
---

#### Image

You can change the image by simply replacing the `favicon.bmp` && `favicon.ico` images.
When setting the bmp image you must ensure that it contains no alpha channel, as the alpha channel is not supported by gdi32.

To change the background color and the color removed from the bmp in order to make it look "transparent", you can modify the following macros in the `main.c` file:

```c
// Defines transparent color of the screensaver
#define IDB_LOGOBITMAP_TRANSPARENT_COLOR RGB(255, 255, 255)

// Defines background color of the screensaver
#define BACKGROUND_COLOR RGB(240, 240, 240)
```

The images are embedded into the executable, therefore you must now recompile the screensaver to apply the changes.


#### Settings

The following registry keys can be used to modify the behavior of the screensaver:

*Registry Path:*

`HKEY_CURRENT_USER\Software\screensaver`

*Values:*

| Key                | Default Value | Description                                              |
|--------------------|---------------|----------------------------------------------------------|
| `cursor_threshold` | 20            | Pixel threshold for cursor movements until the screensaver exits. |
| `image_count`      | 1             | Number of images displayed by the screensaver.           |
| `image_width`      | 0.2           | Image width relative to the window size (1.0 == 100%)    |
| `disable_image_scale` | 0          | If set to 1 the native image size is used (likely better quality), but the image is not scaled based on the window size |
| `image_speed`      | 1             | Speed of the animated images in pixel per frame.         |
| `image_bounce`     | 20            | Bounce intensity of the animated images on collision.    |
| `image_bounce_scale` | 0.7         | Scale factor for bounce decrementation after collision.  |



### Disclaimer
---

This software was built just for fun, 'cause I wanted to try to smoothly synchronizing movement updates with monitor refresh rate on `gdi32`.

To provide a smooth movement animation (accurate update frequency), a spin-lock-like mechanism is used, this works quite well, but is extremely cpu intensive.
Due to this implementation and the overall bad performance of `gdi32` the screensaver uses a notable amount of cpu power (especially when using multiple images per screen).


If you want to build a serious implementation of a screensaver, I highly recommend using graphic engines that can leverage the gpu for rendering (e.g. `Skia`).
(Or directly build it on top of a web-rendering engine (e.g. with `Tauri`) to abstract this all together.)