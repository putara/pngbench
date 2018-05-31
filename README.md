## TL;DR
LodePNG is not so fast. :cry:

## Introduction
PngBench calculates the decompression time of several PNG decoders by using Windows' native icons as test suite.

Currently, the following decoders are supported:

- [GDI+](https://msdn.microsoft.com/en-us/library/windows/desktop/ms533798.aspx)
- [Windows Imaging Component (WIC)](https://msdn.microsoft.com/en-us/library/windows/desktop/ee719902.aspx)
- [libpng](http://www.libpng.org/pub/png/libpng.html) + [zlib](https://zlib.net/)
- [LodePNG & picoPNG](http://lodev.org/lodepng/)
- [uPNG](https://github.com/elanthis/upng)
- [stb_image](https://github.com/nothings/stb)

## Usage
1. Build libpng and zlib.
1. Put `libpng.lib` and `zlib.lib` into `libpng/` folder and `zlib/` folder respectively.
1. Run `nmake` on the developer command prompt.
1. Run either `pngbench_n` or `pngbench_c`.

### Example Result
Decoder|Speed (ms)
-------|---------:
WIC|700
GDI+|945
libpng+zlib|1708
stb_image|2007
LodePNG+zlib|2292
uPNG|2598
LodePNG|3123
picoPNG|3758

## Memo
- WIC has MMX, SSE2 and SSSE3 optimised code for filter and decompression.
- GDI+ just delegates WIC to decode images.
- libpng has SSE2 optimised code for filter.
- zlib has experimental MMX and/or hand-tuned assembly code for decompression.

## Todo
- [ ] Verify decompressed images (maybe something's wrong with results)
- [ ] Add [miniz](https://github.com/richgel999/miniz) support
- [ ] Apply [some SIMD patch](https://www.google.com/search?q=zlib+simd) to zlib

## License
Released under the [WTFPL](http://www.wtfpl.net/about/) except third party libraries.
