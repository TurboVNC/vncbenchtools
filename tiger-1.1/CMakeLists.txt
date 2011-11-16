set(SOURCES
  rdr/Exception.cxx
  rdr/ZlibInStream.cxx
  rdr/ZlibOutStream.cxx
  rfb/PixelBuffer.cxx
  rfb/PixelFormat.cxx)

add_library(tigervnc ${SOURCES})
