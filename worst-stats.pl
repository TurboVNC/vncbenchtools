#!/usr/bin/perl -w

while (<>) {
  if (/\(\s*\d+,\s*\d+\s+(\d+)\*\s*(\d+).*\|\s*(\d+)\|\s*(\d+)$/) {
    my ($w, $h, $zlib, $tight) = ($1, $2, $3, $4);
    if ($w >= 16 && $h >= 16 && $zlib >= 256 && $tight > $zlib * 1.01 + 8) {
      print;
    }
  }
}
