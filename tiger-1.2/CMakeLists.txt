set(SOURCES
  rdr/Exception.cxx
  rdr/ZlibInStream.cxx
  rdr/ZlibOutStream.cxx
  rfb/ComparingUpdateTracker.cxx
  rfb/JpegCompressor.cxx
  rfb/JpegDecompressor.cxx
  rfb/PixelBuffer.cxx
  rfb/PixelFormat.cxx
  rfb/PixelTransformer.cxx
  rfb/Region.cxx
  rfb/TightEncoder.cxx
  rfb/TransImageGetter.cxx
  Xregion/Region.c)

add_library(tigervnc ${SOURCES})
