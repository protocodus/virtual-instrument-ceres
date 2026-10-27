# Third-party notices

Ceres's original source code is covered by the project's `LICENSE` file.
The build downloads or consumes the separate dependencies below; they are not
relicensed by Ceres.

## JUCE 8.0.14

JUCE is Copyright (c) Raw Material Software Limited.

Repository: <https://github.com/juce-framework/JUCE/tree/8.0.14>

A copy of the JUCE licence text is vendored at
[`ThirdParty/JUCE-LICENSE.md`](ThirdParty/JUCE-LICENSE.md) so that it can be
included in every distributed bundle and installer package alongside Ceres's
own licence, as the packaging script does.

License: JUCE framework modules are dual-licensed under the GNU Affero General
Public License version 3 (AGPLv3) and the commercial JUCE licence. Building or
distributing Ceres with JUCE therefore requires either compliance with the
AGPLv3 for the complete combined work or an appropriate commercial JUCE
licence. Review the JUCE 8 licence terms before distribution:
<https://github.com/juce-framework/JUCE/blob/8.0.14/LICENSE.md>

Protocodus's official builds use JUCE under a commercial JUCE 8 Starter
licence. Ceres's own licence is not the AGPLv3, so anyone else who distributes
a build needs their own commercial JUCE licence.

JUCE includes or interfaces with additional third-party components. The
bundled components' licence texts are collected in
[`ThirdParty/JUCE-DEPENDENCY-NOTICES.txt`](ThirdParty/JUCE-DEPENDENCY-NOTICES.txt),
with upstream paths and hashes. This file accompanies each distribution.
Regenerate it from the pinned JUCE checkout using
`Tools/collect_juce_notices.py`; JUCE's own `LICENSE.md` also indexes the sources. The VST3 SDK portions used through JUCE are identified
there as MIT-licensed; Apple's Audio Unit frameworks are supplied by the macOS
SDK and remain subject to Apple's terms.

## CLAP support

The CLAP plug-in uses the MIT-licensed `clap-juce-extensions` wrapper, pinned
to commit `c1a5ad025f95d01e03267857fa8276ebeed16500`:
<https://github.com/free-audio/clap-juce-extensions/tree/c1a5ad025f95d01e03267857fa8276ebeed16500>.
It is Copyright 2019-2020, Paul Walker. The complete upstream licence is
included in [`ThirdParty/CLAP-JUCE-EXTENSIONS-LICENSE.md`](ThirdParty/CLAP-JUCE-EXTENSIONS-LICENSE.md).

The wrapper brings in two dependencies at the revisions recorded by its
submodules:

- CLAP 1.2.7, commit `29ffcc273be7c7c651f6c9953b99e69700e2387a`:
  <https://github.com/free-audio/clap/tree/29ffcc273be7c7c651f6c9953b99e69700e2387a>.
  Its MIT licence is included in [`ThirdParty/CLAP-LICENSE.md`](ThirdParty/CLAP-LICENSE.md).
- CLAP helpers, commit `a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc`:
  <https://github.com/free-audio/clap-helpers/tree/a61bcdf0ecc2c8db1e80bfe8bf9cb7e8d9fd2bbc>.
  Its MIT licence is included in [`ThirdParty/CLAP-HELPERS-LICENSE.md`](ThirdParty/CLAP-HELPERS-LICENSE.md).

Both dependencies are Copyright (c) 2021 Alexandre BIQUE. These licence files
are included in every distribution archive and macOS installer package.

## Arm Optimized Routines (exp2)

The engine's inline `exp2` (`tableExp2` in `Source/DSP/ceres.cpp`, used for
each board's pitch and cutoff factors) follows `math/exp2.c` of Arm Optimized
Routines, Copyright (c) 2018 Arm Limited, with its 128-entry `2^(i/128)`
table: <https://github.com/ARM-software/optimized-routines>. The project is
licensed `MIT OR Apache-2.0 WITH LLVM-exception`; Ceres uses it under the MIT
License, included in
[`ThirdParty/ARM-OPTIMIZED-ROUTINES-LICENSE.md`](ThirdParty/ARM-OPTIMIZED-ROUTINES-LICENSE.md).
The same routine is glibc's and musl's `exp2`.
