// VoLumJackPulseDlopen.cpp
//
// RtAudio calls JACK and PulseAudio by name. This file defines those symbols
// and loads the real libraries with dlopen, so libjack and libpulse are not
// DT_NEEDED. A missing library makes that API fail at runtime; ALSA still works.
// IPlugAPP_linux.cpp probes the same sonames to leave a missing API out of
// Preferences.

#include <dlfcn.h>

#include <jack/jack.h>
#include <pulse/error.h>
#include <pulse/simple.h>

#include <mutex>

namespace
{
void* LoadOnce(const char* primary, const char* fallback)
{
  void* handle = dlopen(primary, RTLD_NOW | RTLD_LOCAL);
  if (!handle && fallback)
    handle = dlopen(fallback, RTLD_NOW | RTLD_LOCAL);
  return handle;
}

void* JackLib()
{
  static void* handle = nullptr;
  static std::once_flag once;
  std::call_once(once, [] { handle = LoadOnce("libjack.so.0", "libjack.so"); });
  return handle;
}

void* PulseSimpleLib()
{
  static void* handle = nullptr;
  static std::once_flag once;
  std::call_once(once, [] { handle = LoadOnce("libpulse-simple.so.0", "libpulse-simple.so"); });
  return handle;
}

void* PulseLib()
{
  static void* handle = nullptr;
  static std::once_flag once;
  std::call_once(once, [] { handle = LoadOnce("libpulse.so.0", "libpulse.so"); });
  return handle;
}

template <typename Fn>
Fn Jack(const char* name)
{
  void* lib = JackLib();
  return lib ? reinterpret_cast<Fn>(dlsym(lib, name)) : nullptr;
}

template <typename Fn>
Fn Pulse(const char* name)
{
  void* simple = PulseSimpleLib();
  void* sym = simple ? dlsym(simple, name) : nullptr;
  if (!sym)
  {
    void* pulse = PulseLib();
    sym = pulse ? dlsym(pulse, name) : nullptr;
  }
  return reinterpret_cast<Fn>(sym);
}
} // namespace

extern "C" {

void jack_set_error_function(void (*func)(const char*))
{
  using Fn = void (*)(void (*)(const char*));
  if (Fn fn = Jack<Fn>("jack_set_error_function"))
    fn(func);
}

jack_client_t* jack_client_open(const char* client_name, jack_options_t options, jack_status_t* status, ...)
{
  using Fn = jack_client_t* (*)(const char*, jack_options_t, jack_status_t*);
  if (Fn fn = Jack<Fn>("jack_client_open"))
    return fn(client_name, options, status);
  if (status)
    *status = JackFailure;
  return nullptr;
}

int jack_client_close(jack_client_t* client)
{
  using Fn = int (*)(jack_client_t*);
  if (Fn fn = Jack<Fn>("jack_client_close"))
    return fn(client);
  return -1;
}

jack_nframes_t jack_get_sample_rate(jack_client_t* client)
{
  using Fn = jack_nframes_t (*)(jack_client_t*);
  if (Fn fn = Jack<Fn>("jack_get_sample_rate"))
    return fn(client);
  return 0;
}

jack_nframes_t jack_get_buffer_size(jack_client_t* client)
{
  using Fn = jack_nframes_t (*)(jack_client_t*);
  if (Fn fn = Jack<Fn>("jack_get_buffer_size"))
    return fn(client);
  return 0;
}

const char** jack_get_ports(jack_client_t* client, const char* port_name_pattern, const char* type_name_pattern,
                            unsigned long flags)
{
  using Fn = const char** (*)(jack_client_t*, const char*, const char*, unsigned long);
  if (Fn fn = Jack<Fn>("jack_get_ports"))
    return fn(client, port_name_pattern, type_name_pattern, flags);
  return nullptr;
}

jack_port_t* jack_port_by_name(jack_client_t* client, const char* port_name)
{
  using Fn = jack_port_t* (*)(jack_client_t*, const char*);
  if (Fn fn = Jack<Fn>("jack_port_by_name"))
    return fn(client, port_name);
  return nullptr;
}

void jack_port_get_latency_range(jack_port_t* port, jack_latency_callback_mode_t mode, jack_latency_range_t* range)
{
  using Fn = void (*)(jack_port_t*, jack_latency_callback_mode_t, jack_latency_range_t*);
  if (Fn fn = Jack<Fn>("jack_port_get_latency_range"))
    fn(port, mode, range);
  else if (range)
    range->min = range->max = 0;
}

int jack_set_process_callback(jack_client_t* client, JackProcessCallback callback, void* arg)
{
  using Fn = int (*)(jack_client_t*, JackProcessCallback, void*);
  if (Fn fn = Jack<Fn>("jack_set_process_callback"))
    return fn(client, callback, arg);
  return -1;
}

int jack_set_xrun_callback(jack_client_t* client, JackXRunCallback callback, void* arg)
{
  using Fn = int (*)(jack_client_t*, JackXRunCallback, void*);
  if (Fn fn = Jack<Fn>("jack_set_xrun_callback"))
    return fn(client, callback, arg);
  return -1;
}

void jack_on_shutdown(jack_client_t* client, JackShutdownCallback callback, void* arg)
{
  using Fn = void (*)(jack_client_t*, JackShutdownCallback, void*);
  if (Fn fn = Jack<Fn>("jack_on_shutdown"))
    fn(client, callback, arg);
}

jack_port_t* jack_port_register(jack_client_t* client, const char* port_name, const char* port_type,
                                unsigned long flags, unsigned long buffer_size)
{
  using Fn = jack_port_t* (*)(jack_client_t*, const char*, const char*, unsigned long, unsigned long);
  if (Fn fn = Jack<Fn>("jack_port_register"))
    return fn(client, port_name, port_type, flags, buffer_size);
  return nullptr;
}

int jack_activate(jack_client_t* client)
{
  using Fn = int (*)(jack_client_t*);
  if (Fn fn = Jack<Fn>("jack_activate"))
    return fn(client);
  return -1;
}

int jack_deactivate(jack_client_t* client)
{
  using Fn = int (*)(jack_client_t*);
  if (Fn fn = Jack<Fn>("jack_deactivate"))
    return fn(client);
  return -1;
}

const char* jack_port_name(const jack_port_t* port)
{
  using Fn = const char* (*)(const jack_port_t*);
  if (Fn fn = Jack<Fn>("jack_port_name"))
    return fn(port);
  return nullptr;
}

int jack_connect(jack_client_t* client, const char* source_port, const char* destination_port)
{
  using Fn = int (*)(jack_client_t*, const char*, const char*);
  if (Fn fn = Jack<Fn>("jack_connect"))
    return fn(client, source_port, destination_port);
  return -1;
}

void* jack_port_get_buffer(jack_port_t* port, jack_nframes_t nframes)
{
  using Fn = void* (*)(jack_port_t*, jack_nframes_t);
  if (Fn fn = Jack<Fn>("jack_port_get_buffer"))
    return fn(port, nframes);
  return nullptr;
}

pa_simple* pa_simple_new(const char* server, const char* name, pa_stream_direction_t dir, const char* dev,
                         const char* stream_name, const pa_sample_spec* ss, const pa_channel_map* map,
                         const pa_buffer_attr* attr, int* error)
{
  using Fn = pa_simple* (*)(const char*, const char*, pa_stream_direction_t, const char*, const char*,
                            const pa_sample_spec*, const pa_channel_map*, const pa_buffer_attr*, int*);
  if (Fn fn = Pulse<Fn>("pa_simple_new"))
    return fn(server, name, dir, dev, stream_name, ss, map, attr, error);
  if (error)
    *error = PA_ERR_CONNECTIONREFUSED;
  return nullptr;
}

void pa_simple_free(pa_simple* s)
{
  using Fn = void (*)(pa_simple*);
  if (Fn fn = Pulse<Fn>("pa_simple_free"))
    fn(s);
}

int pa_simple_write(pa_simple* s, const void* data, size_t bytes, int* error)
{
  using Fn = int (*)(pa_simple*, const void*, size_t, int*);
  if (Fn fn = Pulse<Fn>("pa_simple_write"))
    return fn(s, data, bytes, error);
  if (error)
    *error = PA_ERR_CONNECTIONREFUSED;
  return -1;
}

int pa_simple_read(pa_simple* s, void* data, size_t bytes, int* error)
{
  using Fn = int (*)(pa_simple*, void*, size_t, int*);
  if (Fn fn = Pulse<Fn>("pa_simple_read"))
    return fn(s, data, bytes, error);
  if (error)
    *error = PA_ERR_CONNECTIONREFUSED;
  return -1;
}

int pa_simple_drain(pa_simple* s, int* error)
{
  using Fn = int (*)(pa_simple*, int*);
  if (Fn fn = Pulse<Fn>("pa_simple_drain"))
    return fn(s, error);
  if (error)
    *error = PA_ERR_CONNECTIONREFUSED;
  return -1;
}

int pa_simple_flush(pa_simple* s, int* error)
{
  using Fn = int (*)(pa_simple*, int*);
  if (Fn fn = Pulse<Fn>("pa_simple_flush"))
    return fn(s, error);
  if (error)
    *error = PA_ERR_CONNECTIONREFUSED;
  return -1;
}

const char* pa_strerror(int error)
{
  using Fn = const char* (*)(int);
  if (Fn fn = Pulse<Fn>("pa_strerror"))
    return fn(error);
  return "PulseAudio is not installed";
}

} // extern "C"
