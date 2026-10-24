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

Install the prerequisite build packages for your platform first. The package
manager will also install the matching runtime libraries.

### Debian And Family

On Debian, Ubuntu, and related distributions:

```sh
sudo apt install build-essential pkg-config libshout3-dev libmpg123-dev \
  libmp3lame-dev libvorbis-dev libflac-dev
```

### Fedora

On Fedora:

```sh
sudo dnf install gcc-c++ make pkgconf-pkg-config libshout-devel \
  mpg123-devel lame-devel libvorbis-devel flac-devel
```

### Configure And Install

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

The current build expects all of the audio libraries above to be available.
The `--with-*` options tell `configure` where a dependency is installed when it
is not in the compiler's default search path:

| Configure option | Dependency | Used for |
| --- | --- | --- |
| `--with-shout=DIR` | libshout | Connecting to Icecast-compatible streaming servers |
| `--with-mpg123=DIR` | libmpg123 | MP3 decoding when `--reencode` is enabled |
| `--with-lame=DIR` | libmp3lame | MP3 encoding when `--reencode` or crossfade output is enabled |
| `--with-vorbis=DIR` | libvorbisfile | Ogg Vorbis decoding when `--reencode` is enabled |
| `--with-flac=DIR` | libFLAC | FLAC decoding when `--reencode` is enabled |

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
