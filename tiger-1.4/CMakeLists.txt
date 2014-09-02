set(SOURCES
  rdr/Exception.cxx
  rdr/ZlibInStream.cxx
  rdr/ZlibOutStream.cxx
  rfb/ComparingUpdateTracker.cxx
  rfb/EncodeManager.cxx
  rfb/Encoder.cxx
  rfb/JpegCompressor.cxx
  rfb/JpegDecompressor.cxx
  rfb/PixelBuffer.cxx
  rfb/PixelFormat.cxx
  rfb/Region.cxx
  rfb/TightDecoder.cxx
  rfb/TightEncoder.cxx
  rfb/TightJPEGEncoder.cxx
  rfb/UpdateTracker.cxx
  Xregion/Region.c)

add_library(tigervnc ${SOURCES})
