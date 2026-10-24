// JACK side of smoke-standalone-audio-linux.sh. It feeds a 440 Hz sine at
// -12 dBFS into a client's first input, reads that client's first output and
// counts the xruns the server reports while it runs.
//
//   volum-jack-probe wait-server <timeout s>
//   volum-jack-probe wait-ports <client> <timeout s>
//   volum-jack-probe measure <client> <seconds> [stall ms] [warm-up s]
//
// measure connects, runs the warm-up (printed as "warmup_xruns=N", not part of
// the summary), then prints one line per second ("second=N peak_dbfs=X xruns=N
// dsp_load=X") and the summary of the measured seconds:
//   xruns=N frames=N nonfinite=N peak_dbfs=X active_seconds=N seconds=N max_dsp_load=X
// A stall (red proof) sleeps that long once inside the process callback, two
// seconds into the measured window.

#include <jack/jack.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static jack_client_t* gClient;
static jack_port_t* gOut;
static jack_port_t* gIn;
static volatile int gXruns;
static volatile long gFrames;
static volatile long gNonFinite;
static volatile float gSecondPeak;
static double gPhase;
static double gStep;
static volatile int gStallMs;
static volatile int gMeasuring;

static int OnProcess(jack_nframes_t n, void* arg)
{
  (void)arg;
  float* out = (float*)jack_port_get_buffer(gOut, n);
  const float* in = (const float*)jack_port_get_buffer(gIn, n);
  for (jack_nframes_t i = 0; i < n; ++i)
  {
    out[i] = (float)(0.25 * sin(gPhase));
    gPhase += gStep;
    if (gPhase > 2.0 * M_PI)
      gPhase -= 2.0 * M_PI;
    const float v = in[i];
    if (!isfinite(v))
      ++gNonFinite;
    else if (fabsf(v) > gSecondPeak)
      gSecondPeak = fabsf(v);
  }
  gFrames += n;
  if (gStallMs > 0 && gFrames > 96000 && gMeasuring)
  {
    usleep((useconds_t)gStallMs * 1000);
    gStallMs = 0;
  }
  return 0;
}

static int OnXrun(void* arg)
{
  (void)arg;
  ++gXruns;
  return 0;
}

static jack_client_t* Open(const char* name)
{
  jack_status_t status;
  return jack_client_open(name, JackNoStartServer, &status);
}

static double Dbfs(float v) { return v > 0.f ? 20.0 * log10(v) : -999.0; }

static int PortsExist(jack_client_t* c, const char* client)
{
  char name[256];
  snprintf(name, sizeof(name), "%s:inport 0", client);
  if (!jack_port_by_name(c, name))
    return 0;
  snprintf(name, sizeof(name), "%s:outport 0", client);
  return jack_port_by_name(c, name) != NULL;
}

int main(int argc, char** argv)
{
  if (argc >= 3 && !strcmp(argv[1], "wait-server"))
  {
    for (int t = 0; t < atoi(argv[2]) * 10; ++t)
    {
      jack_client_t* c = Open("volum-probe-wait");
      if (c)
      {
        printf("server up: %u Hz, %u frames\n", jack_get_sample_rate(c), jack_get_buffer_size(c));
        jack_client_close(c);
        return 0;
      }
      usleep(100000);
    }
    fprintf(stderr, "no JACK server\n");
    return 1;
  }

  if (argc >= 4 && !strcmp(argv[1], "wait-ports"))
  {
    jack_client_t* c = Open("volum-probe-wait");
    if (!c)
      return 1;
    for (int t = 0; t < atoi(argv[3]) * 10; ++t)
    {
      if (PortsExist(c, argv[2]))
      {
        jack_client_close(c);
        return 0;
      }
      usleep(100000);
    }
    jack_client_close(c);
    fprintf(stderr, "no ports for JACK client %s\n", argv[2]);
    return 1;
  }

  if (argc >= 4 && !strcmp(argv[1], "measure"))
  {
    const char* client = argv[2];
    const int seconds = atoi(argv[3]);
    gStallMs = argc >= 5 ? atoi(argv[4]) : 0;
    gClient = Open("volum-probe");
    if (!gClient)
      return 1;
    gStep = 2.0 * M_PI * 440.0 / (double)jack_get_sample_rate(gClient);
    gOut = jack_port_register(gClient, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    gIn = jack_port_register(gClient, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    jack_set_process_callback(gClient, OnProcess, NULL);
    jack_set_xrun_callback(gClient, OnXrun, NULL);
    if (!gOut || !gIn || jack_activate(gClient))
      return 1;

    char target[256];
    snprintf(target, sizeof(target), "%s:inport 0", client);
    if (jack_connect(gClient, jack_port_name(gOut), target))
    {
      fprintf(stderr, "cannot connect to %s\n", target);
      return 1;
    }
    snprintf(target, sizeof(target), "%s:outport 0", client);
    if (jack_connect(gClient, target, jack_port_name(gIn)))
    {
      fprintf(stderr, "cannot connect from %s\n", target);
      return 1;
    }

    const int warmup = argc >= 6 ? atoi(argv[5]) : 0;
    if (warmup > 0)
      sleep((unsigned)warmup);
    printf("warmup_seconds=%d warmup_xruns=%d\n", warmup, gXruns);
    fflush(stdout);
    gXruns = 0;
    gFrames = 0;
    gNonFinite = 0;
    gMeasuring = 1;

    float peak = 0.f, maxLoad = 0.f;
    int active = 0, xrunsBefore = 0;
    for (int s = 1; s <= seconds; ++s)
    {
      gSecondPeak = 0.f;
      sleep(1);
      const float secondPeak = gSecondPeak;
      const int xruns = gXruns;
      const float load = jack_cpu_load(gClient);
      if (load > maxLoad)
        maxLoad = load;
      if (secondPeak > peak)
        peak = secondPeak;
      if (Dbfs(secondPeak) > -50.0)
        ++active;
      printf("second=%d peak_dbfs=%.1f xruns=%d dsp_load=%.1f\n", s, Dbfs(secondPeak), xruns - xrunsBefore, load);
      xrunsBefore = xruns;
      fflush(stdout);
    }
    jack_deactivate(gClient);
    jack_client_close(gClient);
    printf("xruns=%d frames=%ld nonfinite=%ld peak_dbfs=%.1f active_seconds=%d seconds=%d max_dsp_load=%.1f\n", gXruns,
           gFrames, gNonFinite, Dbfs(peak), active, seconds, maxLoad);
    return 0;
  }

  fprintf(stderr, "usage: volum-jack-probe wait-server <s> | wait-ports <client> <s> | measure <client> <s> [stall ms]\n");
  return 2;
}
