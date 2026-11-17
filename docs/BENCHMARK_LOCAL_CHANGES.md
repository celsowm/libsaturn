# Local checkout review evidence

All new optimization return runners must call
`local_changes.capture(ROOT)` before building and
`local_changes.add_to_zip(z, snapshot)` in the final archive, including
failed runs. Existing static projection and hierarchy runners use the
same helper. A resumed run records the current checkout at resume time,
not the historical source of retained binaries.

The `local-changes/` directory contains HEAD, status, diff statistics,
combined tracked patch, staged patch, unstaged patch and a manifest of
untracked nonignored files. Small UTF-8 source files in known source
directories are included under `untracked/`; generated files, assets,
BIOS, hidden paths, symlinks and likely credential filenames are excluded
from this automatic copy. All other untracked paths are listed only.
Tracked binary deltas are not embedded. These restrictions keep the
review bundle focused; the manifest declares the policy. This is not a
secret scanner: review tracked patches and source contents before sharing.

No Git mutation is performed. Use the bundle to decide whether to commit
local edits separately from a benchmark. Optional standalone capture
without emulators:

```sh
python3 harness/scripts/local_changes.py --out harness/build/local-changes-return.zip
```
