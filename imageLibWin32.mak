################################################################################
# 
# imageConsole Makefile
#
################################################################################

PROJECT_FILE_NAME_STEM = imageConsole

!INCLUDE ..\buildingBlocks\makeFileCommonWin32.mak


################################################################################
### Build Targets

ALL : "$(OUTDIR)\imageConsole.exe"

CLEAN :
   $(DELETE_BUILD_OUTPUT_FILES)

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"



################################################################################
### Dependencies

## I use MSVCRTD, so prevent any other file/library from dragging in LIBCMTD.
## Those two libraries will try to export the same symbols.
LIB32_FLAGS=$(COMMON_LINK_FLAGS) \
            /SUBSYSTEM:CONSOLE \
            /out:"$(OUTDIR)\imageConsole.exe" \
            /NODEFAULTLIB:LIBCMTD \
            $(COMMON_LINK_LIBRARIES) \
            WS2_32.Lib \
            kernel32.lib \
            Advapi32.lib \
            User32.lib \
            Oleaut32.lib \
            ole32.lib

LIB32_OBJS= \
      "$(OUTDIR)\consoleMain.obj" \
      "$(OUTDIR)\lineDetection.obj" \
      "$(OUTDIR)\bioCAD2DImage.obj" \
      "$(OUTDIR)\edgeDetection.obj" \
      "$(OUTDIR)\bioGeometry.obj" \
      "$(OUTDIR)\bioCADStatsFile.obj" \
      "$(OUTDIR)\plyFileFormat.obj" \
      "$(OUTDIR)\bmpParser.obj" \
      "$(OUTDIR)\perfMetrics.obj" \
      "..\basicServer\Debug\basicServer.lib" \
      "..\BuildingBlocks\Debug\buildingBlocks.lib"


"$(OUTDIR)\imageConsole.exe" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    @$(LINK) $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS) 



# Create a dependency from object files onto header files.
# Unfortunately, I cannot use a wildcard in the target,
# so I have to make a separate target for each .c file.
# Moreover, I cannot easily specify which header files a target
# depends on without manually updating the makefile each time
# I add or remove an #include statement. Visual Studio does
# this; it reads the .cpp files and locates all #include files,
# and then generates a separate <proj>.dep file, which records these
# dependencies. This file is then included in the makefile.
# That's too risky to do manually, so instead I'm making a blanket 
# dependency on all header files.
"$(OUTDIR)\consoleMain.obj" : .\*.cpp
"$(OUTDIR)\lineDetection.obj" : .\*.cpp
"$(OUTDIR)\bioCAD2DImage.obj" : .\*.cpp
"$(OUTDIR)\edgeDetection.obj" : .\*.cpp
"$(OUTDIR)\bioGeometry.obj" : .\*.cpp
"$(OUTDIR)\bioCADStatsFile.obj" : .\*.cpp
"$(OUTDIR)\plyFileFormat.obj" : .\*.cpp
"$(OUTDIR)\bmpParser.obj" : .\*.cpp
"$(OUTDIR)\perfMetrics.obj" : .\*.cpp


## WARNING! Do NOT put a blank line above here. It will be interpreted as
## an empty rule for the last .obj file, so nmake will do 
## nothing to create that last .obj file.





