// X side of smoke-standalone-audio-linux.sh. Xvfb runs no window manager, so
// this sends the WM_DELETE_WINDOW a window manager sends for the close button.
//
//   volum-x-tool ping                             exit 0 when $DISPLAY accepts a connection
//   volum-x-tool wait <window name> <timeout s>   exit 0 once a top-level window has that name
//   volum-x-tool close <window name>              WM_DELETE_WINDOW to every such window

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// _NET_WM_NAME first (GDK always sets it, as UTF-8), then WM_NAME.
static int HasName(Display* d, Window w, const char* name)
{
  Atom type;
  int format, match = 0;
  unsigned long items, after;
  unsigned char* data = NULL;
  const Atom netName = XInternAtom(d, "_NET_WM_NAME", False);
  const Atom utf8 = XInternAtom(d, "UTF8_STRING", False);
  if (XGetWindowProperty(d, w, netName, 0, 256, False, utf8, &type, &format, &items, &after, &data) == Success && data)
  {
    match = !strcmp((const char*)data, name);
    XFree(data);
    if (match)
      return 1;
  }
  char* title = NULL;
  if (XFetchName(d, w, &title) && title)
  {
    match = !strcmp(title, name);
    XFree(title);
  }
  return match;
}

static int ForEachNamed(Display* d, const char* name, int close)
{
  Window root = DefaultRootWindow(d), parent, *children = NULL;
  unsigned int count = 0;
  int found = 0;
  if (!XQueryTree(d, root, &root, &parent, &children, &count))
    return 0;
  const Atom protocols = XInternAtom(d, "WM_PROTOCOLS", False);
  const Atom deleteWindow = XInternAtom(d, "WM_DELETE_WINDOW", False);
  for (unsigned int i = 0; i < count; ++i)
  {
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(d, children[i], &attrs) || attrs.map_state != IsViewable)
      continue;
    if (HasName(d, children[i], name))
    {
      ++found;
      if (close)
      {
        XEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = children[i];
        ev.xclient.message_type = protocols;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = (long)deleteWindow;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(d, children[i], False, NoEventMask, &ev);
      }
    }
  }
  if (children)
    XFree(children);
  XFlush(d);
  return found;
}

int main(int argc, char** argv)
{
  Display* d = XOpenDisplay(NULL);
  if (!d)
  {
    if (argc < 2 || strcmp(argv[1], "ping"))
      fprintf(stderr, "volum-x-tool: cannot open display\n");
    return 2;
  }
  if (argc >= 2 && !strcmp(argv[1], "ping"))
    return 0;
  if (argc >= 4 && !strcmp(argv[1], "wait"))
  {
    for (int t = 0; t < atoi(argv[3]) * 10; ++t)
    {
      if (ForEachNamed(d, argv[2], 0))
        return 0;
      usleep(100000);
    }
    return 1;
  }
  if (argc >= 3 && !strcmp(argv[1], "close"))
  {
    const int n = ForEachNamed(d, argv[2], 1);
    printf("WM_DELETE_WINDOW sent to %d window(s) named %s\n", n, argv[2]);
    return n ? 0 : 1;
  }
  fprintf(stderr, "usage: volum-x-tool wait <name> <s> | close <name>\n");
  return 2;
}
