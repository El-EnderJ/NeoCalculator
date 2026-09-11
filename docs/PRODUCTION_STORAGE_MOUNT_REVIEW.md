# Production mount failure policy

Reviewed for EQUATIONS-TUTOR-CLOSEOUT-01 against the installed Arduino 2.0.17
package `3.20017.241212+sha.dcc1105b` (ESP-IDF 4.4.7). The test runner records the
actual `LittleFS.cpp` hash; this review does not substitute current upstream docs
for that pinned implementation.

`LittleFSFS::begin(bool formatOnFail, ...)` first checks whether the partition is
already mounted. Otherwise it registers with `format_if_mount_failed=false`.
Only `ESP_FAIL` together with `formatOnFail=true` invokes `format()` and retries.
Any remaining error returns false before publishing the mountpoint.

Ordinary WROOM-1U startup and production demo pass **false**. The legacy CAM
startup branch remains unchanged. Runtime CircuitCore, Fluid2D and NeoLanguage
mount retries now also pass false, including their Arduino CAM builds. Successful
mounts retain the existing record formats and loading behavior. Explicit demo
factory/provisioning operations remain separate and unchanged.

`main::setup()` calls `SystemApp::begin()` after display/LVGL setup. The legacy
VariableContext loads NVS read-only; it does not mount LittleFS. App constructors
defer their screen/persistence initialization. The launcher is created and loaded
before the LittleFS attempt. SystemApp owns `_filesystemMounted`: success sets
it true and loads VariableManager and production settings; failure sets it false,
skips those loaders, emits `LittleFS FAIL (continuing without persistence)`, and
returns to the usable launcher. Setup continues to keypad/serial initialization.
There is no retry/reset loop. A BOOT OK line describes completed application
startup, not a successful filesystem mount.

Startup no longer creates a dummy one-byte `vars.dat` after a failed existence
check. Missing, inaccessible and invalid records are reported without that write.
Loaders return failure for unavailable files. Valid settings still apply their
existing brightness migration when applicable; this is not a mount-failure
recovery operation. Later explicit user saves retain their existing semantics.
Hidden app launch paths can reach persistence helpers; their eight former
format-on-failure retries are closed. No recovery UI or format migration is added.

Run `python tests/host/run_production_storage_tests.py`. It compiles the actual
pinned `begin` body, actual ordinary startup filesystem block, production settings
load/save bodies and VariableManager against fake VFS/I/O dependencies. Cases
cover valid records, mount failure, another error code, downstream unavailability,
empty mounted storage, invalid records, and injected uninitialized/corrupt mount
errors. Assertions require true mount-state propagation, zero implicit formats,
zero startup writes/removes/renames, unchanged stored bytes, useful diagnostics,
bounded return, and valid variable/settings restoration. Launcher ordering and
the eight retry sites are separately labelled static checks.

If installed, the pinned `mklittlefs` tool additionally reads a valid and an empty
64 KiB disposable image, rejects erased and damaged metadata images, and leaves
every input byte unchanged. Its assertion/error exit on damaged input is retained
as expected negative evidence. This tests that host image reader, **not** ESP32
flash corruption or every real storage failure. No physical port, flash, filesystem
format or security operation is used by these tests. Generated sources, images,
hashes and results remain under ignored `out/storage-startup`.

The change adds no persistent objects, global arena or renderer code. Removing
the dummy file creation removes that startup file-handle scope; full stack/heap
cost is not inferred from source or static RAM. The previously hardware-tested
firmware predates this closeout's retry/diagnostic fixes and does not validate
their exact binary. Per-snapshot build/test results are recorded in the closeout
ledger.
