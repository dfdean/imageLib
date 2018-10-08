#############################################################################
#
# Copyright (c) 2010-2018 Dawson Dean
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
#############################################################################
#
# Linux Makefile for building ImageLib
#
# This expects to run in the top of the project directory, so relative all
# paths assume that other project directories are ../otherProjectName.
#############################################################################



#############################################################################
# Compiler, tools and options
#   -c will generate a library, not a final executable.
#   -g will include debug information.
#   -pg will include gprof profiling.
#
CC = g++
CFLAGS = -c -Wall -W -g \
   -fno-strength-reduce -static \
   -D"LINUX" \
   -DLINUX \
   -D"_DEBUG" \
   -DDD_DEBUG=1 \
   -DINCLUDE_REGRESSION_TESTS=1 \
   -DGOTO_DEBUGGER_ON_WARNINGS=1 \
   -DCONSISTENCY_CHECKS=1 

# For WASM, I build with portableBuildingBlocks
INCPATH = -I$(QTDIR)/include -I../buildingBlocks 
##INCPATH = -I$(QTDIR)/include

LINK = ar
LFLAGS = rcs
LIBS = 

OUTPUT_DIR = obj


#############################################################################
# Files

HEADERS =

SOURCES = lineDetection.cpp \
   imageEditor.cpp \
   edgeDetection.cpp \
   bioGeometry.cpp \
   plyFileFormat.cpp \
   bmpParser.cpp \
   excelFile.cpp \
   perfMetrics.cpp

OBJECTS = \
      $(OUTPUT_DIR)/lineDetection.o \
      $(OUTPUT_DIR)/imageEditor.o \
      $(OUTPUT_DIR)/edgeDetection.o \
      $(OUTPUT_DIR)/bioGeometry.o \
      $(OUTPUT_DIR)/plyFileFormat.o \
      $(OUTPUT_DIR)/bmpParser.o \
      $(OUTPUT_DIR)/excelFile.o \
      $(OUTPUT_DIR)/perfMetrics.o


TARGET = $(OUTPUT_DIR)/libImageLib.a


#############################################################################
# Implicit rules

.SUFFIXES: .cpp .c

$(OUTPUT_DIR)/%.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<


#############################################################################
# Build rules

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LINK) $(LFLAGS) $(TARGET) $(OBJECTS)

clean:
	-rm -f $(OBJECTS) $(TARGET)
	-rm -f ~/core


#############################################################################
# File Rules
# Create a dependency from object files onto header files.
$(OUTPUT_DIR)/lineDetection.o: lineDetection.cpp
$(OUTPUT_DIR)/imageEditor.o: imageEditor.cpp
$(OUTPUT_DIR)/edgeDetection.o: edgeDetection.cpp
$(OUTPUT_DIR)/bioGeometry.o: bioGeometry.cpp
$(OUTPUT_DIR)/plyFileFormat.o: plyFileFormat.cpp
$(OUTPUT_DIR)/bmpParser.o: bmpParser.cpp
$(OUTPUT_DIR)/excelFile.o: excelFile.cpp
$(OUTPUT_DIR)/perfMetrics.o: perfMetrics.cpp
