#!/usr/bin/env python3
"""Load one VST3 in Carla's single-client engine, process, and close.

Ubuntu's /usr/bin/carla-single is the plug-in bridge: it execs
carla-bridge-native and waits for a host. It does not process audio.
carla-jack-single is the single-client host (ENGINE_PROCESS_MODE_SINGLE_CLIENT
on libcarla_standalone2). This gate drives that engine. The caller starts
`jackd -d dummy` and exports JACK_DEFAULT_SERVER before launching Python.
"""

import ctypes
import os
import sys
import time

PLUGIN_VST3 = 6
BINARY_POSIX64 = 2
PLUGIN_OPTIONS_NULL = 0x10000
ENGINE_OPTION_PROCESS_MODE = 1
ENGINE_PROCESS_MODE_SINGLE_CLIENT = 0
ENGINE_OPTION_FORCE_STEREO = 3


def load_lib():
    candidates = [
        "/usr/lib/carla/libcarla_standalone2.so",
        "/usr/lib/x86_64-linux-gnu/carla/libcarla_standalone2.so",
    ]
    for path in candidates:
        if os.path.isfile(path):
            return ctypes.CDLL(path), path
    print("carla-single: libcarla_standalone2.so not found", file=sys.stderr)
    return None, None


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: carla-single.py /path/VoLum.vst3", file=sys.stderr)
        return 2
    bundle = os.path.abspath(sys.argv[1])
    if not os.path.isdir(bundle):
        print(f"carla-single: bundle missing: {bundle}", file=sys.stderr)
        return 2
    server = os.environ.get("JACK_DEFAULT_SERVER", "")
    if not server:
        print("carla-single: JACK_DEFAULT_SERVER is unset", file=sys.stderr)
        return 2

    lib, path = load_lib()
    if lib is None:
        return 2
    print(f"carla-single: {path} server={server} bundle={bundle}")

    lib.carla_standalone_host_init.restype = ctypes.c_void_p
    lib.carla_set_engine_option.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_char_p]
    lib.carla_set_engine_buffer_size_and_sample_rate.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_double]
    lib.carla_set_engine_buffer_size_and_sample_rate.restype = ctypes.c_bool
    lib.carla_engine_init.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
    lib.carla_engine_init.restype = ctypes.c_bool
    lib.carla_engine_close.argtypes = [ctypes.c_void_p]
    lib.carla_engine_close.restype = ctypes.c_bool
    lib.carla_engine_idle.argtypes = [ctypes.c_void_p]
    lib.carla_is_engine_running.argtypes = [ctypes.c_void_p]
    lib.carla_is_engine_running.restype = ctypes.c_bool
    lib.carla_get_last_error.argtypes = [ctypes.c_void_p]
    lib.carla_get_last_error.restype = ctypes.c_char_p
    lib.carla_add_plugin.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_int64,
        ctypes.c_void_p,
        ctypes.c_uint,
    ]
    lib.carla_add_plugin.restype = ctypes.c_bool
    lib.carla_get_current_plugin_count.argtypes = [ctypes.c_void_p]
    lib.carla_get_current_plugin_count.restype = ctypes.c_uint
    lib.carla_set_active.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_bool]
    lib.carla_transport_play.argtypes = [ctypes.c_void_p]
    lib.carla_get_current_transport_frame.argtypes = [ctypes.c_void_p]
    lib.carla_get_current_transport_frame.restype = ctypes.c_uint64
    lib.carla_get_real_plugin_name.argtypes = [ctypes.c_void_p, ctypes.c_uint]
    lib.carla_get_real_plugin_name.restype = ctypes.c_char_p

    host = lib.carla_standalone_host_init()
    if not host:
        print("carla-single: carla_standalone_host_init failed", file=sys.stderr)
        return 1
    lib.carla_set_engine_option(host, ENGINE_OPTION_PROCESS_MODE, ENGINE_PROCESS_MODE_SINGLE_CLIENT, b"")
    lib.carla_set_engine_option(host, ENGINE_OPTION_FORCE_STEREO, 1, b"")
    lib.carla_set_engine_buffer_size_and_sample_rate(host, 256, 48000.0)
    if not lib.carla_engine_init(host, b"JACK", b"VoLumCarla"):
        err = lib.carla_get_last_error(host)
        print(f"carla-single: engine init failed: {err}", file=sys.stderr)
        return 1
    try:
        if not lib.carla_is_engine_running(host):
            print("carla-single: engine is not running", file=sys.stderr)
            return 1
        added = lib.carla_add_plugin(
            host,
            BINARY_POSIX64,
            PLUGIN_VST3,
            bundle.encode(),
            b"VoLum",
            b"",
            0,
            None,
            PLUGIN_OPTIONS_NULL,
        )
        if not added:
            err = lib.carla_get_last_error(host)
            print(f"carla-single: add_plugin failed: {err}", file=sys.stderr)
            return 1
        count = lib.carla_get_current_plugin_count(host)
        name = lib.carla_get_real_plugin_name(host, 0)
        name_s = name.decode("utf-8", "replace") if name else ""
        print(f"carla-single: loaded count={count} name={name_s!r}")
        if count != 1 or "VoLum" not in name_s:
            print("carla-single: plugin did not load as VoLum", file=sys.stderr)
            return 1
        lib.carla_set_active(host, 0, True)
        lib.carla_transport_play(host)
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            lib.carla_engine_idle(host)
            time.sleep(0.05)
        frame = lib.carla_get_current_transport_frame(host)
        print(f"carla-single: transport frame {frame}")
        if frame <= 0:
            print("carla-single: engine did not process", file=sys.stderr)
            return 1
        print("carla-single: PASS")
        return 0
    finally:
        lib.carla_engine_close(host)


if __name__ == "__main__":
    sys.exit(main())
