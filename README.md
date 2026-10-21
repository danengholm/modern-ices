# modern-ices

`modern-ices` is a small C++23 reimplementation of the MP3 streaming core of
ices 0.4. It keeps the practical dependency stack for MP3 streaming:

- libshout for Icecast/Shoutcast compatible output
- libmpg123 for MP3 decoding when transcoding is enabled
- libmp3lame for MP3 encoding when transcoding is enabled

The code is organized around RAII classes instead of global C state. Library
calls are checked and converted into exceptions that are reported cleanly by
`main()`.

## Build

```sh
./configure --prefix=/usr/local
make
make install
```

If dependencies are installed in a non-default prefix:

```sh
CPPFLAGS="-I/opt/ice/include" LDFLAGS="-L/opt/ice/lib" ./configure --prefix=/opt/ice
make
```

## Run

Create a playlist with one MP3 path per line:

```sh
printf '%s\n' /path/to/file.mp3 > playlist.txt
```

Stream files without transcoding:

```sh
modern-ices -F playlist.txt -h localhost -p 8000 -P hackme -m /stream
```

Decode with mpg123 and re-encode with LAME at 128 kbps:

```sh
modern-ices -F playlist.txt -R -b 128
```

Use `modern-ices --help` for all supported options.
