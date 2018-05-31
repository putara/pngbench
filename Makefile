INTDIR = .\bin\obj
OUTDIR = .\bin
CC = cl.exe
LD = link.exe
LDFLAGS = /nologo /dynamicbase:no /machine:x86 /map /nxcompat /opt:icf /opt:ref /debug /pdbaltpath:%_PDB%
CFLAGS_N = /nologo /c /GF /GR- /GS- /Gy /MD /O2b2 /W3 /fp:fast /Zi /Fd"$(INTDIR)/"
CFLAGS_N = $(CFLAGS_N) /D "_STATIC_CPPLIB" /D "NDEBUG" /D "WIN32" /D "_WIN32" /D "_WINDOWS"
CFLAGS_N = $(CFLAGS_N) /D "LODEPNG_NO_COMPILE_ENCODER" /D "LODEPNG_NO_COMPILE_DISK" /D "LODEPNG_NO_COMPILE_ERROR_TEXT" /D "LODEPNG_NO_COMPILE_CPP"
CFLAGS = $(CFLAGS_N) /Fo"$(INTDIR)/"

all: $(OUTDIR) $(INTDIR) "$(OUTDIR)\pngbench_c.exe" "$(OUTDIR)\pngbench_n.exe"

clean:
 -@rmdir /s /q "$(INTDIR)"
 -@rmdir /s /q "$(OUTDIR)"

"$(INTDIR)" :
 @if not exist "$(INTDIR)" mkdir "$(INTDIR)"

"$(OUTDIR)" :
 @if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

.SUFFIXES: .c .cpp .obj .exe

OBJS = "$(INTDIR)\testimgs.obj" "$(INTDIR)\lodepng.obj" "$(INTDIR)\picopng.obj" "$(INTDIR)\upng.obj"

"$(OUTDIR)\pngbench_n.exe": "$(INTDIR)\pngbench_n.obj" $(OBJS)
 $(LD) $(LDFLAGS) /out:$@ "$(INTDIR)\pngbench_n.obj" $(OBJS)

"$(OUTDIR)\pngbench_c.exe": "$(INTDIR)\pngbench_c.obj" $(OBJS)
 $(LD) $(LDFLAGS) /out:$@ "$(INTDIR)\pngbench_c.obj" $(OBJS)

"$(OUTDIR)\maketest.exe": "$(INTDIR)\maketest.obj"
 $(LD) $(LDFLAGS) /out:$@ "$(INTDIR)\maketest.obj"

.c{$(INTDIR)}.obj::
 $(CC) $(CFLAGS) $<

.cpp{$(INTDIR)}.obj::
 $(CC) $(CFLAGS) $<

"$(INTDIR)\pngbench_c.obj": "pngbench.cpp"
 $(CC) $(CFLAGS_N) /Fo$@ /D "SUPPORT_COLOUR_CONVERSION" "pngbench.cpp"

"$(INTDIR)\pngbench_n.obj": "pngbench.cpp"
 $(CC) $(CFLAGS_N) /Fo$@ "pngbench.cpp"

"$(INTDIR)\lodepng.obj": "lodepng\lodepng.cpp" "lodepng\lodepng.h"
 $(CC) $(CFLAGS) "lodepng\lodepng.cpp"

"$(INTDIR)\picopng.obj": "lodepng\picopng.cpp" "lodepng\picopng.h"
 $(CC) $(CFLAGS) "lodepng\picopng.cpp"

"$(INTDIR)\upng.obj": "upng\upng.c" "upng\upng.h"
 $(CC) $(CFLAGS) "upng\upng.c"

"$(INTDIR)\testimgs.obj": "$(OUTDIR)\testimgs.c" "$(OUTDIR)\testimgs.h"
 $(CC) $(CFLAGS) "$(OUTDIR)\testimgs.c"

"$(OUTDIR)\testimgs.c": "$(OUTDIR)\testimgs.h"

"$(OUTDIR)\testimgs.h": "$(OUTDIR)\maketest.exe"
 "$(OUTDIR)\maketest.exe" "$(OUTDIR)\testimgs.c" "$(OUTDIR)\testimgs.h"

pngbench.cpp: "$(OUTDIR)\testimgs.h" "lodepng\lodepng.h" "lodepng\picopng.h" "upng\upng.h"
