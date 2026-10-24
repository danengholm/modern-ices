# modern-ices

`modern-ices` is a small C++23 reimplementation of the streaming and
transcoding core of ices 0.4. It keeps the practical dependency stack used by
that program:

- libshout for Icecast/Shoutcast compatible output
- libmpg123 for MP3 decoding when transcoding is enabled
- libvorbisfile for Ogg Vorbis decoding when transcoding is enabled
- libFLAC for FLAC decoding when transcoding is enabled
- libmp3lame for MP3 encoding when transcoding is enabled

The code is organized around RAII classes instead of global C state. Library
calls are checked and converted into exceptions that are reported cleanly by
`main()`.

## Original Project Credit

`modern-ices` is a derivative work inspired by and based on the design of the
original IceS/ices 0.x source client from the Xiph.Org Icecast project. The
original ices 0.x program provided the MP3-oriented Icecast source-client model,
playlist-driven streaming, transcoding support, and crossfade behavior that this
project reimplements in modern C++.

The historical ices0 source tree is kept by Xiph for reference:

```text
http://svn.xiph.org/icecast/trunk/ices0/
```

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

Equivalent explicit dependency prefixes are also supported:

```sh
./configure --with-shout=/opt/ice --with-mpg123=/opt/ice --with-lame=/opt/ice \
  --with-vorbis=/opt/ice --with-flac=/opt/ice
```

## Run

Create a playlist with one audio path per line:

```sh
printf '%s\n' /path/to/file.mp3 /path/to/file.ogg /path/to/file.flac > playlist.txt
```

Stream MP3 files without transcoding:

```sh
modern-ices -F playlist.txt -h localhost -p 8000 -P hackme -m /stream
```

Decode MP3, Ogg Vorbis, or FLAC and re-encode with LAME at 128 kbps:

```sh
modern-ices -F playlist.txt -R -b 128
```

Crossfade between decoded tracks:

```sh
modern-ices -F playlist.txt -R -x 5
```

Prepend timestamps to log output with the default `strftime` format `%G`:

```sh
modern-ices -F playlist.txt -s
```

Use a custom `strftime` format:

```sh
modern-ices -F playlist.txt -s '%Y-%m-%d %H:%M:%S'
```

Use `modern-ices --help` for all supported options.

## Development Process

This project is being developed collaboratively with Codex in a vibe-coding
workflow: the human maintainer sets direction, tests against real audio and
server setups, and reviews behavior and style, while Codex helps translate that
intent into C++23 code, build-system updates, verification runs, and focused
follow-up fixes.

Changes should stay easy to review. Prefer small commits with one concern each,
keep formatting-only work separate from behavior changes, run `clang-format`
before committing C++ files, and verify with `make` plus an Icecast smoke test
when networking is available.
