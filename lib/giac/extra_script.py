# GIAC-A01: native-only build tweaks for the vendored Giac library.
#
# Firmware (espressif32) is untouched: the Arduino platform already injects
# HAVE_CONFIG_H globally and manages its own optimization/link flags. On the
# native platform (emulator_pc) this scopes the Windows-only workarounds to
# Giac TUs so the rest of the emulator keeps full warnings/errors:
#   - __MINGW_H: Giac guards its POSIX access() calls with it, but the macro
#     is only set once a Windows header is included (the compiler predefine
#     is __MINGW32__); defining it up front makes the guards work in TUs
#     that never touch Windows headers (ksym2poly.cc, kidentificateur.cc).
#   - -fpermissive: one LP64 pointer -> unsigned long debug print in
#     kusual.cc that LLP64 Windows rejects as an error.
#   - -O1 -w: the emulator env sets no optimization level; unoptimized Giac
#     is disproportionately slow, and its warning volume would drown real
#     project warnings.
Import("env")
import sys

if env.get("PIOPLATFORM") == "native":
    env.Append(CCFLAGS=["-O1", "-w"])
    if sys.platform.startswith(("win", "cygwin", "msys")):
        env.Append(CPPDEFINES=["__MINGW_H"])
        env.Append(CXXFLAGS=["-fpermissive"])
    else:
        # config.h #undefs HAVE_UNISTD_H (embedded profile), but the
        # non-Windows branch of Giac's access() guard still calls POSIX
        # access(); force the declaration in every Giac TU.
        env.Append(CCFLAGS=["-include", "unistd.h"])
        if sys.platform == "darwin":
            # Apple libc++ removed the legacy binder declarations that this
            # vendored Giac snapshot still names. Restore declarations only;
            # keep emulator_pc's C++17/optimization/link semantics unchanged.
            env.Append(CPPDEFINES=[
                "_LIBCPP_ENABLE_CXX17_REMOVED_BINDERS",
                ("SIZEOF_VOID_P", 4),
            ])
            env.Append(CXXFLAGS=["-Wno-register", "-Wno-c++11-narrowing"])
