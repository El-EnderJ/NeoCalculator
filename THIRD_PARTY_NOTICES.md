# Third-party notices

NumOS is a multi-license distribution. This file records vendored and generated
components plus material build-time dependencies. Each component remains under
its own license; inclusion here does not relicense it.

## Vendored source

### Giac/KhiCAS

- Location: `lib/giac/`
- Upstream: Bernard Parisse's Giac, via the KhiCAS `ti-ce-giac` port
- Upstream repository: <https://github.com/KhiCAS/ti-ce-giac>
- Best-established baseline: `8d24f392f3edcb4fbf44b11325e92ca37edee470`
- License: GPL-3.0-or-later
- License text: `LICENSE-SOFTWARE`
- Local provenance and modification record: `lib/giac/NUMOS_CHANGES.md`

The snapshot is modified. The exact pre-import source package remains
unresolved, as documented in the modification record.

### LibTomMath

- Location: `lib/libtommath/`
- Upstream baseline: libtommath commit
  `652d70a31fcecc7beb7a01b3edef645039f54778` (`v1.3.0-416-g652d70a`)
- Upstream repository: <https://github.com/libtom/libtommath>
- Verification: all 170 shared source paths at import commit `321aac87` were
  byte-identical to that upstream commit
- License: the LibTom license (public-domain dedication / Unlicense text)
- License text: `lib/libtommath/LICENSE`

`native_random_stubs.c` is a later NumOS compatibility file under
GPL-3.0-or-later; it is not represented as upstream LibTomMath code.

### Lua

- Location: `src/lua/`
- Upstream: Lua 5.4.7
- Official release: <https://www.lua.org/ftp/lua-5.4.7.tar.gz>
- Source tag: <https://github.com/lua/lua/tree/v5.4.7>
- Release archive SHA-256:
  `9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30`
- Verification: 59 core files are byte-identical to the official release
  archive; all 63 pre-existing local paths are byte-identical to source tag
  `v5.4.7` (`1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3`)
- Copyright: Copyright (C) 1994-2024 Lua.org, PUC-Rio
- License: MIT
- License text: `src/lua/LICENSE`

The four paths outside the release archive comparison are `all`, `ltests.h`,
`manual/2html`, and `manual/manual.of`; each exactly matches the source tag.
The subset omits other upstream files and is not represented as a complete
release tree.

### TinyMT

- Location: `lib/giac/src/tinymt32_license.h` (license notice) and associated
  TinyMT material in the Giac vendor tree
- Upstream project: <https://github.com/MersenneTwister-Lab/TinyMT>
- Copyright: Copyright (c) 2011 Mutsuo Saito, Makoto Matsumoto, Hiroshima
  University, and The University of Tokyo
- License: BSD-3-Clause
- License text: preserved in `lib/giac/src/tinymt32_license.h`

The header records modifications by Bernard Parisse. Its exact pre-Giac
TinyMT revision was not established, so no commit identifier is asserted.

### CLAPACK/f2c headers

- Locations: `lib/giac/src/f2c.h`, `lib/giac/src/clapack.h`, and
  `lib/giac/src/blaswrap.h`
- Proximate upstream: CLAPACK 3.2.1 CMAKE distribution,
  <https://www.netlib.org/clapack/clapack-3.2.1-CMAKE.tgz>
- Release archive SHA-256:
  `0b3f782bc24845d85f36bafbff0f2f1384dc72df730fda4e7924ec1a70baca5a`
- License: BSD-3-Clause
- License text: `lib/giac/CLAPACK-COPYING`

The three local headers derive from the corresponding CLAPACK 3.2.1 headers
but contain Giac-side type, linkage, and wrapper changes. Their exact
pre-Giac transformation revision is not asserted. They are byte-identical to
the identified `ti-ce-giac` baseline, so NumOS did not add those changes.

### GL2PS header

- Location: `lib/giac/src/gl2ps.h`
- Upstream repository: <https://gitlab.onelab.info/gl2ps/gl2ps>
- Closest release: tag `gl2ps_1_3_5`
- Copyright: Copyright (C) 1999-2009 Christophe Geuzaine
- License: the header offers LGPL-2.0-or-later or the alternative GL2PS license
- License texts: `lib/giac/COPYING.LGPL` and `lib/giac/COPYING.GL2PS`
- Notice: the upstream copyright and dual-license grant remain in the header

This header is part of the vendor snapshot and is not enabled by the current
embedded configuration (`HAVE_LIBGL2PS` is not defined). It is byte-identical
to the identified `ti-ce-giac` baseline. Its code matches GL2PS 1.3.5; the
only release-header difference is a later boilerplate replacement of the
FSF postal-address paragraph with a web link.

## Font software and generated derivatives

### Montserrat

- Source: `assets/fonts/Montserrat-Regular.ttf`
- SHA-256: `ef9cf99f0175bef530b88934bd904fcf56f773cec6fd4dfb8ccaf7ce2bbd395e`
- Embedded copyright: Copyright 2011 The Montserrat Project Authors
- License: OFL-1.1
- License text: `assets/fonts/LICENSES/Montserrat-OFL-1.1.txt`
- Derived outputs: `src/fonts/lv_font_montserrat_math_12.c` and
  `src/fonts/lv_font_montserrat_math_14.c`

The repository font reports version 9.000, but its exact pre-import download
artifact was not located. It is therefore not attributed to a guessed tag or
URL. The adjacent notice uses the copyright and OFL grant embedded in this
binary, followed by the canonical OFL 1.1 text. See `assets/fonts/README.md`.

### STIX Two Math

- Source: `assets/fonts/STIXTwoMath-Regular.ttf`
- SHA-256: `562551b15b836e6e01d1b7350909baf3c8c8d83260c1190fbf4544333e6936de`
- Copyright: Copyright 2001-2021 The STIX Fonts Project Authors
- Provenance: byte-identical to Google Fonts commit
  `76d97cb481958f9bb0976a453f8126f8e2ea87ab`, path
  `ofl/stixtwomath/STIXTwoMath-Regular.ttf`
- Exact source: <https://github.com/google/fonts/blob/76d97cb481958f9bb0976a453f8126f8e2ea87ab/ofl/stixtwomath/STIXTwoMath-Regular.ttf>
- License: OFL-1.1; Reserved Font Name `TM Math`
- License text: `assets/fonts/LICENSES/STIXTwoMath-OFL-1.1.txt`
- Derived outputs: `src/fonts/stix_math_{8,12,18}.c` and
  `src/math/font/stix_math_{constants,italics,variants}.h`

The generated files carry OFL provenance notices. NumOS-authored table
scaffolding in the generated headers is marked
`GPL-3.0-or-later AND OFL-1.1`; the glyph/font data itself is marked OFL-1.1.

## Package-manager and build-time dependencies

`platformio.ini` resolves LVGL and TFT_eSPI through PlatformIO rather than
vendoring them here. Their source distributions and license notices are
provided by their respective packages. The declarations are currently not
fully locked (`lvgl/lvgl@^9.2.0` and unpinned `bodmer/TFT_eSPI`), so this audit
does not assert one resolved source hash.

`lv_font_conv` is an MIT-licensed build-time tool. It is not redistributed as
source in this repository. The generated font files record the invoking script
and retain the font licenses that govern their embedded font data.

Other ESP-IDF/Arduino/PlatformIO framework components are obtained by the
selected toolchain and are not represented as vendored NumOS source.
