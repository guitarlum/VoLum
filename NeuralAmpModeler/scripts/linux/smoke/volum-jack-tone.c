/* A steady tone into the VoLum JACK input, for the OUT-meter gate.
   Connects the first input port whose name contains "VoLum". */
#include <jack/jack.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static jack_port_t* gPort;
static jack_nframes_t gRate = 48000;
static double gPhase;

static int Process(jack_nframes_t n, void* arg)
{
  (void)arg;
  float* buf = (float*)jack_port_get_buffer(gPort, n);
  const double step = 2.0 * 3.141592653589793 * 440.0 / (double)gRate;
  jack_nframes_t i;
  for (i = 0; i < n; ++i)
  {
    buf[i] = (float)(0.45 * sin(gPhase));
    gPhase += step;
    if (gPhase > 2.0 * 3.141592653589793)
      gPhase -= 2.0 * 3.141592653589793;
  }
  return 0;
}

int main(void)
{
  jack_client_t* client = jack_client_open("volum-tone", JackNullOption, NULL);
  if (!client)
  {
    fprintf(stderr, "volum-jack-tone: cannot open JACK\n");
    return 1;
  }
  gRate = jack_get_sample_rate(client);
  jack_set_process_callback(client, Process, NULL);
  gPort = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  if (!gPort || jack_activate(client))
  {
    fprintf(stderr, "volum-jack-tone: activate failed\n");
    return 1;
  }
  const char** ports = jack_get_ports(client, "VoLum", JackPortIsInput, 0);
  if (!ports || !ports[0])
  {
    fprintf(stderr, "volum-jack-tone: no VoLum input port\n");
    return 1;
  }
  if (jack_connect(client, jack_port_name(gPort), ports[0]))
  {
    fprintf(stderr, "volum-jack-tone: connect failed %s\n", ports[0]);
    return 1;
  }
  fprintf(stderr, "volum-jack-tone: %s\n", ports[0]);
  for (;;)
    sleep(60);
}
