// X side of smoke-standalone-audio-linux.sh. Xvfb runs no window manager, so
// this sends the WM_DELETE_WINDOW a window manager sends for the close button.
//
//   volum-x-tool ping                             exit 0 when $DISPLAY accepts a connection
//   volum-x-tool wait <window name> <timeout s>   exit 0 once a top-level window has that name
//   volum-x-tool close <window name>              WM_DELETE_WINDOW to every such window
//   volum-x-tool child <window name>              id x y w h of the largest viewable descendant

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
    match = strstr((const char*)data, name) != NULL;
    XFree(data);
    if (match)
      return 1;
  }
  char* title = NULL;
  if (XFetchName(d, w, &title) && title)
  {
    match = strstr(title, name) != NULL;
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

// Deepest viewable descendant that is large enough to be the editor. A same-size
// wrapper sits above the plug-in window; sending keys to the wrapper never
// reaches it, because XSendEvent propagates to ancestors, not children.
static Window LargestChild(Display* d, Window top, int* ox, int* oy, unsigned int* ow, unsigned int* oh)
{
  typedef struct
  {
    Window w;
    int depth;
  } Node;
  Node stack[64];
  int n = 0;
  stack[n].w = top;
  stack[n].depth = 0;
  n++;
  Window best = 0;
  unsigned int bestArea = 0;
  int bestDepth = -1;
  int bx = 0, by = 0;
  unsigned int bw = 0, bh = 0;
  while (n > 0)
  {
    Node node = stack[--n];
    Window root, parent, *children = NULL;
    unsigned int count = 0;
    if (!XQueryTree(d, node.w, &root, &parent, &children, &count))
      continue;
    for (unsigned int i = 0; i < count; ++i)
    {
      XWindowAttributes attrs;
      if (!XGetWindowAttributes(d, children[i], &attrs) || attrs.map_state != IsViewable)
        continue;
      if (n < 64)
      {
        stack[n].w = children[i];
        stack[n].depth = node.depth + 1;
        n++;
      }
      if (attrs.width < 800 || attrs.height < 500)
        continue;
      const unsigned int area = (unsigned int)attrs.width * (unsigned int)attrs.height;
      const int depth = node.depth + 1;
      if (depth > bestDepth || (depth == bestDepth && area > bestArea))
      {
        best = children[i];
        bestArea = area;
        bestDepth = depth;
        bx = attrs.x;
        by = attrs.y;
        bw = (unsigned int)attrs.width;
        bh = (unsigned int)attrs.height;
      }
    }
    if (children)
      XFree(children);
  }
  *ox = bx;
  *oy = by;
  *ow = bw;
  *oh = bh;
  return best;
}

static int SendKey(Display* d, Window w, const char* name, int shift)
{
  KeySym ks = XStringToKeysym(name);
  if (!ks)
    return 1;
  KeyCode kc = XKeysymToKeycode(d, ks);
  if (!kc)
    return 1;
  int x = 1, y = 1;
  XKeyEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.display = d;
  ev.window = w;
  ev.root = DefaultRootWindow(d);
  ev.keycode = kc;
  ev.state = shift ? ShiftMask : 0;
  ev.time = CurrentTime;
  ev.same_screen = True;
  ev.x = x;
  ev.y = y;
  ev.x_root = x;
  ev.y_root = y;
  ev.type = KeyPress;
  XSendEvent(d, w, False, KeyPressMask, (XEvent*)&ev);
  ev.type = KeyRelease;
  XSendEvent(d, w, False, KeyReleaseMask, (XEvent*)&ev);
  XFlush(d);
  return 0;
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
  if (argc >= 3 && !strcmp(argv[1], "child"))
  {
    Window root = DefaultRootWindow(d), parent, *children = NULL;
    unsigned int count = 0;
    if (!XQueryTree(d, root, &root, &parent, &children, &count))
      return 1;
    for (unsigned int i = 0; i < count; ++i)
    {
      XWindowAttributes attrs;
      if (!XGetWindowAttributes(d, children[i], &attrs) || attrs.map_state != IsViewable)
        continue;
      if (!HasName(d, children[i], argv[2]))
        continue;
      int x, y;
      unsigned int w, h;
      Window child = LargestChild(d, children[i], &x, &y, &w, &h);
      if (!child)
        child = children[i], w = (unsigned int)attrs.width, h = (unsigned int)attrs.height, x = attrs.x, y = attrs.y;
      printf("%lu %d %d %u %u\n", (unsigned long)child, x, y, w, h);
      XFree(children);
      return 0;
    }
    if (children)
      XFree(children);
    return 1;
  }
  if (argc >= 4 && !strcmp(argv[1], "key"))
  {
    Window w = (Window)strtoul(argv[2], NULL, 10);
    const int shift = argc >= 5 && !strcmp(argv[4], "shift");
    if (SendKey(d, w, argv[3], shift))
    {
      fprintf(stderr, "volum-x-tool: unknown key %s\n", argv[3]);
      return 1;
    }
    return 0;
  }
  fprintf(stderr, "usage: volum-x-tool wait <name> <s> | close <name> | child <name> | key <id> <keysym> [shift]\n");
  return 2;
}
