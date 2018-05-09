# [essentia~]

`[essentia~]` is a Max wrapper around [Essentia](http://essentia.upf.edu/),
a Music Information Retrieval library.

**NOTE**: Currently only builds for 64-bit architecture.


## Developing

### Installing dependencies

#### Max SDK

[Download the Max SDK](https://github.com/Cycling74/max-sdk).

In Max, make sure to include the SDK's `externals` directory in
**Options** > **File Preferences...**

#### Essentia

[Install Essentia](http://essentia.upf.edu/documentation/installing.html).

On macOS, [install Homebrew](https://brew.sh/) then `tap` and install Essentia
using the [`homebrew-essentia` formula](https://github.com/MTG/homebrew-essentia).
(Xcode should successfully locate `libessentia.dylib` in `/usr/local/lib`.)

### Building

Build from the Xcode project.
