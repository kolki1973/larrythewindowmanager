 /* larry.c [ 0.0.1]
 *
 *  Started from catwm 31/12/10
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#define _BSD_SOURCE
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
/* If you have a multimedia keyboard uncomment the following line */
//#include <X11/XF86keysym.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <locale.h>
#include <string.h>

#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define TABLENGTH(X)    (sizeof(X)/sizeof(*X))

typedef union {
    char *com[256];
    int i;
} Arg;

// Structs
typedef struct {
    unsigned int mod;
    char * keysym;
    void (*myfunction)(const Arg arg);
    Arg arg;
} key;

typedef struct client client;
struct client{
    // Prev and next client
    client *next;
    client *prev;

    // The window
    Window win;
    unsigned int order;
};

typedef struct desktop desktop;
struct desktop{
    unsigned int master_size, mode, growth, numwins, nmaster;
    client *head, *current, *transient;
};

typedef struct {
    unsigned long wincolor;
    GC gc;
} Theme;

typedef struct {
    char *name;
    char *list[100];
} Commands;

typedef struct {
    char *class;
    unsigned int desk, follow;
} WORKSPACE;

// Functions
static void add_window(Window w, int tw);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void change_desktop(const Arg arg);
static void client_to_desktop(const Arg arg);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void follow_client_to_desktop(const Arg arg);
static unsigned long getcolor(const char* color);
static void grabkeys();
static void init_desks(unsigned int ws);
static void keypress(XEvent *e);
static void kill_client();
static void kill_client_now(Window w);
static void last_desktop();
static void logger(const char* e);
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void more_master(const Arg arg);
static void move_down(const Arg arg);
static void move_up(const Arg arg);
static void next_win();
static void prev_win();
static void quit();
static void remove_window(Window w, unsigned int dr, unsigned int tw);
static void read_keys_file();
static void read_rcfile();
static void resize_master(const Arg arg);
static void resize_stack(const Arg arg);
static void rotate_desktop(const Arg arg);
static void save_desktop(int i);
static void select_desktop(int i);
static void setup();
static void set_defaults();
static void sigchld(int unused);
static void spawn(const Arg arg);
static void start();
static void swap_master();
static void switch_mode(const Arg arg);
static void tile();
static void toggle_bar();
static void unmapnotify(XEvent *e);    // Thunderbird's write window just unmaps...
static void update_config();
static void update_current();
static void update_info();
static void warp_pointer();

// Variable
static Display *dis;
static unsigned int attachaside, bdw, bool_quit, clicktofocus, current_desktop;
static unsigned int followmouse, mode, msize, previous_desktop, DESKTOPS;
static int growth, sh, sw, master_size, nmaster;
static unsigned int screen, bar_height, BAR_HEIGHT, doinfo;
static unsigned int topbar, top_stack, keycount, cmdcount, wspccount;
static int ufalpha;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
unsigned int numlockmask;        /* dynamic key lock mask */
static Window root;
static client *head, *current, *transient;
static char RC_FILE[100], KEY_FILE[100];
static Atom alphaatom, wm_delete_window, protos, *protocols;
static XWindowAttributes attr;
static const char *defaultwincolor[] = { "#ff0000", "#00ff00", };

// Events array
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [UnmapNotify] = unmapnotify,
    [ButtonPress] = buttonpress,
    [MotionNotify] = motionnotify,
    [ButtonRelease] = buttonrelease,
    [DestroyNotify] = destroynotify,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest
};

// Desktop array
static desktop desktops[10];
static Theme theme[2];
static key keys[100];
static Commands cmds[50];
static WORKSPACE wspc[20];
#include "readconfs.c"

/* ***************************** Window Management ******************************* */
void add_window(Window w, int tw) {
    client *c,*t, *dummy;

    if(!(c = (client *)calloc(1,sizeof(client)))) {
        logger("\033[0;31mError calloc!");
        exit(1);
    }

    c->win = w; c->order = 0;
    dummy = (tw == 1) ? transient : head;

    if(dummy == NULL) {
        c->next = NULL; c->prev = NULL;
        dummy = c;
    } else {
        for(t=dummy;t;t=t->next)
            ++t->order;
        if(attachaside == 0) {
            if(top_stack == 0) {
                c->next = dummy->next; c->prev = dummy;
                dummy->next = c;
            } else {
                for(t=dummy;t->next;t=t->next); // Start at the last in the stack
                t->next = c; c->next = NULL;
                c->prev = t;
            }
        } else {
            c->prev = NULL; c->next = dummy;
            c->next->prev = c;
            dummy = c;
        }
    }

    if(tw == 1) {
        transient = dummy;
        save_desktop(current_desktop);
        return;
    } else head = dummy;
    current = c;
    desktops[current_desktop].numwins += 1;
    if(growth > 0) growth = growth*(desktops[current_desktop].numwins-1)/desktops[current_desktop].numwins;
    else growth = 0;
    save_desktop(current_desktop);
    // for folow mouse 
    if(followmouse == 0)
        XSelectInput(dis, c->win, PointerMotionMask);
}

void remove_window(Window w, unsigned int dr, unsigned int tw) {
    client *c, *t, *dummy;

    dummy = (tw == 1) ? transient : head;
    for(c=dummy;c;c=c->next) {
        if(c->win == w) {
            if(c->prev == NULL && c->next == NULL) {
                dummy = NULL;
            } else if(c->prev == NULL) {
                dummy = c->next;
                c->next->prev = NULL;
            } else if(c->next == NULL) {
                c->prev->next = NULL;
            } else {
                c->prev->next = c->next;
                c->next->prev = c->prev;
            } break;
        }
    }
    if(tw == 1) {
        transient = dummy;
        free(c);
        save_desktop(current_desktop);
        update_current();
        return;
    } else {
        head = dummy;
        XUngrabButton(dis, AnyButton, AnyModifier, c->win);
        XUnmapWindow(dis, c->win);
        desktops[current_desktop].numwins -= 1;
        if(head != NULL) {
            for(t=head;t;t=t->next) {
                if(t->order > c->order) --t->order;
                if(t->order == 0) current = t;
            }
        } else current = NULL;
        if(dr == 0) free(c);
        if(desktops[current_desktop].numwins < 3) growth = 0;
        else growth = growth*(desktops[current_desktop].numwins-1)/desktops[current_desktop].numwins;
        if(nmaster > 0 && nmaster == (desktops[current_desktop].numwins-1)) nmaster -= 1;
        save_desktop(current_desktop);
        tile();
        warp_pointer();
        update_current();
        return;
    }
}

void next_win() {
    client *c;

    if(desktops[current_desktop].numwins < 2) return;
    if(mode == 1) XUnmapWindow(dis, current->win);
    c = (current->next == NULL) ? head : current->next;

    current = c;
    save_desktop(current_desktop);
    if(mode == 1) tile();
    update_current();
    warp_pointer();
}

void prev_win() {
    client *c;

    if(desktops[current_desktop].numwins < 2) return;
    if(mode == 1) XUnmapWindow(dis, current->win);
    if(current->prev == NULL) for(c=head;c->next;c=c->next);
    else c = current->prev;

    current = c;
    save_desktop(current_desktop);
    if(mode == 1) tile();
    update_current();
    warp_pointer();
}

void move_down(const Arg arg) {
    Window tmp;
    if(current == NULL || current->next == NULL || current->win == head->win || current->prev == NULL)
        return;

    tmp = current->win;
    current->win = current->next->win;
    current->next->win = tmp;
    //keep the moved window activated
    next_win();
    update_current();
    save_desktop(current_desktop);
    tile();
}

void move_up(const Arg arg) {
    Window tmp;
    if(current == NULL || current->prev == head || current->win == head->win) {
        return;
    }
    tmp = current->win;
    current->win = current->prev->win;
    current->prev->win = tmp;
    prev_win();
    save_desktop(current_desktop);
    tile();
}

void swap_master() {
    Window tmp;

    if(head->next != NULL && current != NULL && mode != 1) {
        if(current == head) {
            tmp = head->next->win;
            head->next->win = head->win;
            head->win = tmp;
        } else {
            tmp = head->win;
            head->win = current->win;
            current->win = tmp;
            current = head;
        }
        save_desktop(current_desktop);
        tile();
        warp_pointer();
        update_current();
    }
}

/* **************************** Desktop Management ************************************* */

void update_info() {
    if(doinfo > 0) return;
    int i, j;

    fflush(stdout);
    for(i=0;i<DESKTOPS;++i) {
        j = (i == current_desktop) ? 1 : 0;
        printf("%d:%d:%d ", i, desktops[i].numwins, j);
    }
    printf("%d\n", mode);
    fflush(stdout);
}

void change_desktop(const Arg arg) {
    if(arg.i >= DESKTOPS || arg.i == current_desktop) return;
    client *c;

    // Save current "properties"
    save_desktop(current_desktop);
    previous_desktop = current_desktop;

    // Unmap all window
    if(transient != NULL)
        for(c=transient;c;c=c->next)
            XUnmapWindow(dis,c->win);

    if(head != NULL)
        for(c=head;c;c=c->next)
            XUnmapWindow(dis,c->win);

    // Take "properties" from the new desktop
    select_desktop(arg.i);

    // Map all windows
    if(head != NULL)
        if(mode != 1)
            for(c=head;c;c=c->next)
                XMapWindow(dis,c->win);

    if(transient != NULL)
        for(c=transient;c;c=c->next)
            XMapWindow(dis,c->win);

    tile();
    warp_pointer();
    update_current();
    update_info();
}

void last_desktop() {
    Arg a = {.i = previous_desktop};
    change_desktop(a);
}

void rotate_desktop(const Arg arg) {
    Arg a = {.i = (current_desktop + DESKTOPS + arg.i) % DESKTOPS};
     change_desktop(a);
}

void follow_client_to_desktop(const Arg arg) {
    if(arg.i >= DESKTOPS) return;
    client_to_desktop(arg);
    change_desktop(arg);
}

void client_to_desktop(const Arg arg) {
    if(arg.i == current_desktop || current == NULL ||arg.i >= DESKTOPS) return;

    client *tmp = current;
    unsigned int tmp2 = current_desktop;

    // Add client to desktop
    select_desktop(arg.i);
    add_window(tmp->win, 0);
    save_desktop(arg.i);

    select_desktop(tmp2);
    // Remove client from current desktop
    remove_window(current->win, 0, 0);
    update_info();
}

void save_desktop(int i) {
    desktops[i].master_size = master_size;
    desktops[i].nmaster = nmaster;
    desktops[i].mode = mode;
    desktops[i].growth = growth;
    desktops[i].head = head;
    desktops[i].current = current;
    desktops[i].transient = transient;
}

void select_desktop(int i) {
    master_size = desktops[i].master_size;
    nmaster = desktops[i].nmaster;
    mode = desktops[i].mode;
    growth = desktops[i].growth;
    head = desktops[i].head;
    current = desktops[i].current;
    transient = desktops[i].transient;
    current_desktop = i;
}

void more_master (const Arg arg) {
    if(arg.i > 0) {
        if((desktops[current_desktop].numwins < 3) || (nmaster == (desktops[current_desktop].numwins-2))) return;
        nmaster += 1;
    } else {
        if(nmaster == 0) return;
        nmaster -= 1;
    }
    save_desktop(current_desktop);
    tile();
    //update_current();
}

void tile() {
    if(head == NULL) return;
    client *c, *d=NULL;
    unsigned int x = 0, ypos=0;
    int y = 0, n = 0;

    // For a top bar
    y = (bar_height > 0 && topbar == 0) ? bar_height : 0; ypos = y;

    // If only one window
    if(head != NULL && head->next == NULL) {
        if(mode == 1) XMapWindow(dis, current->win);
        XMoveResizeWindow(dis,head->win,0,y,sw+bdw,sh+bdw);
    } else {
        if(mode == 0) { /* Vertical */
            // Master window
            if(nmaster < 1)
                XMoveResizeWindow(dis,head->win,0,y,master_size - bdw,sh - bdw);
            else {
                for(d=head;d;d=d->next) {
                    XMoveResizeWindow(dis,d->win,0,ypos,master_size - bdw,sh/(nmaster+1) - bdw);
                    if(x == nmaster) break;
                    ypos += sh/(nmaster+1); ++x;
                }
            }
            // Stack
            if(d == NULL) d = head;
            n = desktops[current_desktop].numwins - (nmaster+1);
            XMoveResizeWindow(dis,d->next->win,master_size + bdw,y,sw-master_size-(2*bdw),(sh/n)+growth - bdw);
            y += (sh/n)+growth;
            for(c=d->next->next;c;c=c->next) {
                XMoveResizeWindow(dis,c->win,master_size + bdw,y,sw-master_size-(2*bdw),(sh/n)-(growth/(n-1)) - bdw);
                y += (sh/n)-(growth/(n-1));
            }
        } else { /* Fullscreen */
                XMoveResizeWindow(dis,current->win,0,y,sw+2*bdw,sh+2*bdw);
                XMapWindow(dis, current->win);
        }
    }
}

void update_current() {
    client *c; unsigned int border;
    unsigned long opacity = (ufalpha/100.00) * 0xffffffff;

    if(head == NULL) return;
    border = ((head->next == NULL) || (mode == 1)) ? 0 : bdw;
    for(c=head;c;c=c->next) {
        XSetWindowBorderWidth(dis,c->win,border);
        if(c != current || transient != NULL) {
            if(c->order < current->order) ++c->order;
            if(ufalpha < 100) XChangeProperty(dis, c->win, alphaatom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &opacity, 1l);
            XSetWindowBorder(dis,c->win,theme[1].wincolor);
            if(clicktofocus == 0)
                XGrabButton(dis, AnyButton, AnyModifier, c->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
        }
        else {
            // "Enable" current window
            if(ufalpha < 100) XDeleteProperty(dis, c->win, alphaatom);
            XSetWindowBorder(dis,c->win,theme[0].wincolor);
            XSetInputFocus(dis,c->win,RevertToParent,CurrentTime);
            XRaiseWindow(dis,c->win);
            if(clicktofocus == 0)
                XUngrabButton(dis, AnyButton, AnyModifier, c->win);
        }
    }
    current->order = 0;
    if(transient != NULL) {
        XSetInputFocus(dis,transient->win,RevertToParent,CurrentTime);
        XRaiseWindow(dis,transient->win);
    }

    XSync(dis, False);
}

void switch_mode(const Arg arg) {
    client *c;

    if(mode == arg.i) return;
    growth = 0;
    if(mode == 1 && head != NULL) {
        XUnmapWindow(dis, current->win);
        for(c=head;c;c=c->next)
            XMapWindow(dis, c->win);
    }

    mode = arg.i;
    master_size = (sw*msize)/100;
    if(mode == 1 && head != NULL)
        for(c=head;c;c=c->next)
            XUnmapWindow(dis, c->win);

    save_desktop(current_desktop);
    tile();
    update_current();
    update_info();
}

void resize_master(const Arg arg) {
    if(arg.i > 0) {
        if(sw-master_size > 70)
            master_size += arg.i;
    } else if(master_size > 70) master_size += arg.i;
    tile();
}

void resize_stack(const Arg arg) {
    if(desktops[current_desktop].numwins > 2) {
        int n = desktops[current_desktop].numwins-1;
        if(arg.i > 0) {
            if(sh-(growth+sh/n) > (n-1)*70)
                growth += arg.i;
        } else {
            if((sh/n+growth) > 70)
                growth += arg.i;
        }
        tile();
    }
}


void toggle_bar() {
    if(BAR_HEIGHT > 0) {
        if(bar_height >0) {
            sh += bar_height;
            bar_height = 0;
        } else {
            bar_height = BAR_HEIGHT;
            sh -= bar_height;
        }
        tile();
    }
}

/* ********************** Keyboard Management ********************** */
void grabkeys() {
    unsigned int i;
    int j;
    XModifierKeymap *modmap;
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    read_keys_file();
    // numlock workaround
    numlockmask = 0;
    modmap = XGetModifierMapping(dis);
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < modmap->max_keypermod; ++j) {
            if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dis, XK_Num_Lock))
                numlockmask = (1 << i);
        }
    }
    XFreeModifiermap(modmap);

    // For each shortcuts
    for(i=0;i<keycount;++i) {
        code = XKeysymToKeycode(dis,XStringToKeysym(keys[i].keysym));
        XGrabKey(dis, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    }
    for(i=1;i<4;i+=2) {
        XGrabButton(dis, i, Mod1Mask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, Mod1Mask | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, Mod1Mask | numlockmask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, Mod1Mask | numlockmask | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;

    keysym = XkbKeycodeToKeysym(dis, (KeyCode)ev->keycode, 0, 0);
    for(i = 0; i < keycount; ++i) {
        if(keysym == XStringToKeysym(keys[i].keysym) && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
            if(keys[i].myfunction)
                keys[i].myfunction(keys[i].arg);
        }
    }
}

void warp_pointer() {
    // Move cursor to the center of the current window
    if(followmouse != 0) return;
    XGetWindowAttributes(dis, current->win, &attr);
    XWarpPointer(dis, None, current->win, 0, 0, 0, 0, attr.width/2, attr.height/2);
    return;
}

void configurenotify(XEvent *e) {
    // Do nothing for the moment
}

/* ********************** Signal Management ************************** */
void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    int y = (bar_height > 0 && topbar == 0) ? bar_height : 0;

    wc.x = ev->x;
    wc.y = ev->y + y;
    wc.width = (ev->width < sw-bdw) ? ev->width : sw+bdw;
    wc.height = (ev->height < sh-bdw) ? ev->height : sh+bdw;
    wc.border_width = 0;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dis, ev->window, ev->value_mask, &wc);
    XSync(dis, False);
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;

    XGetWindowAttributes(dis, ev->window, &attr);
    if(attr.override_redirect == True) return;

    int y = (bar_height > 0 && topbar == 0) ? bar_height : 0;
    // For fullscreen mplayer (and maybe some other program)
    client *c;
    for(c=head;c;c=c->next)
        if(ev->window == c->win) {
            XMapWindow(dis,ev->window);
            return;
        }

    Window trans = None;
    if (XGetTransientForHint(dis, ev->window, &trans) && trans != None) {
        add_window(ev->window, 1); 
        if((attr.y + attr.height) > sh)
            XMoveResizeWindow(dis,ev->window,attr.x,y,attr.width,attr.height-10);
        XSetWindowBorderWidth(dis,ev->window,bdw);
        XSetWindowBorder(dis,ev->window,theme[0].wincolor);
        update_current();
        XMapRaised(dis,ev->window);
        XSetInputFocus(dis,ev->window,RevertToParent,CurrentTime);
        return;
    }

    XClassHint ch = {0};
    unsigned int i=0, j=0, tmp = current_desktop;
    if(XGetClassHint(dis, ev->window, &ch))
        for(i=0;i<wspccount;++i)
            if(strcmp(ch.res_class, wspc[i].class) == 0) {
                save_desktop(tmp);
                select_desktop(wspc[i].desk-1);
                for(c=head;c;c=c->next)
                    if(ev->window == c->win)
                        ++j;
                if(j < 1) add_window(ev->window, 0);
                if(tmp == wspc[i].desk-1) {
                    tile();
                    XMapWindow(dis, ev->window);
                    update_current();
                } else select_desktop(tmp);
                if(wspc[i].follow < 1) {
                    Arg a = {.i = wspc[i].desk-1};
                    change_desktop(a);
                }
                if(ch.res_class) XFree(ch.res_class);
                if(ch.res_name) XFree(ch.res_name);
                update_info();
                return;
            }
    if(ch.res_class) XFree(ch.res_class);
    if(ch.res_name) XFree(ch.res_name);

    add_window(ev->window, 0);
    tile();
    if(mode != 1) XMapWindow(dis,ev->window);
    warp_pointer();
    update_current();
    update_info();
}

void destroynotify(XEvent *e) {
    unsigned int i = 0, tmp = current_desktop;
    client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    save_desktop(tmp);
    for(i=current_desktop;i<current_desktop+DESKTOPS;++i) {
        select_desktop(i%DESKTOPS);
        if(transient != NULL) {
            for(c=transient;c;c=c->next)
                if(ev->window == c->win) {
                    remove_window(ev->window, 0, 1);
                    select_desktop(tmp);
                    return;
                }
        }
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                remove_window(ev->window, 0, 0);
                select_desktop(tmp);
                update_info();
                return;
            }
    }
    select_desktop(tmp);
}

void buttonpress(XEvent *e) {
    XButtonEvent *ev = &e->xbutton;
    client *c;

    // change focus with LMB
    if(clicktofocus == 0 && ev->window != current->win && ev->button == Button1)
        for(c=head;c;c=c->next) {
            if(ev->window == c->win) {
                current = c;
                update_current();
                XSendEvent(dis, PointerWindow, False, 0xfff, e);
                XFlush(dis);
                return;
            }
        }
}

void motionnotify(XEvent *e) {
    client *c;
    XMotionEvent *ev = &e->xmotion;

    if(ev->window != current->win) {
        for(c=head;c;c=c->next)
           if(ev->window == c->win) {
                current = c;
                update_current();
                return;
           }
    }
}

void buttonrelease(XEvent *e) {
    XSendEvent(dis, PointerWindow, False, 0xfff, e);
    XFlush(dis);
    return;
}

void unmapnotify(XEvent *e) { // for thunderbird's write window and maybe others
    XUnmapEvent *ev = &e->xunmap;
    unsigned int i = 0, tmp = current_desktop;
    client *c;

    if(ev->send_event == 1) {
        if(transient != NULL) {
            for(c=transient;c;c=c->next)
                if(ev->window == c->win) {
                    remove_window(ev->window, 1, 1);
                    select_desktop(tmp);
                    return;
                }
        }
        save_desktop(tmp);
        for(i=0;i<TABLENGTH(desktops);++i) {
            select_desktop(i);
            for(c=head;c;c=c->next)
                if(ev->window == c->win) {
                    remove_window(ev->window, 1, 0);
                    select_desktop(tmp);
                    update_info();
                    return;
                }
        }
        select_desktop(tmp);
    }
}

void kill_client() {
    if(head == NULL) return;
    kill_client_now(current->win);
    remove_window(current->win, 0, 0);
    update_info();
}

void kill_client_now(Window w) {
    int n, i;
    XEvent ke;

    if (XGetWMProtocols(dis, w, &protocols, &n) != 0) {
        for(i=n;i>=0;--i) {
            if (protocols[i] == wm_delete_window) {
                ke.type = ClientMessage;
                ke.xclient.window = w;
                ke.xclient.message_type = protos;
                ke.xclient.format = 32;
                ke.xclient.data.l[0] = wm_delete_window;
                ke.xclient.data.l[1] = CurrentTime;
                XSendEvent(dis, w, False, NoEventMask, &ke);
            }
        }
    } else XKillClient(dis, w);
    XFree(protocols);
}

void quit() {
    unsigned int i;
    client *c;

    for(i=0;i<DESKTOPS;++i) {
        if(desktops[i].head != NULL) select_desktop(i);
        else continue;
        for(c=head;c;c=c->next)
            kill_client_now(c->win);
    }
    logger("\033[0;34mYou Quit : Bye!");
    XClearWindow(dis, root);
    XUngrabKey(dis, AnyKey, AnyModifier, root);
    for(i=0;i<7;++i)
        XFreeGC(dis, theme[i].gc);
    XSync(dis, False);
    XSetInputFocus(dis, root, RevertToPointerRoot, CurrentTime);
    bool_quit = 1;
}

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(dis,screen);

    if(!XAllocNamedColor(dis,map,color,&c,&c)) {
        logger("\033[0;31mError parsing color!");
        return 1;
    }
    return c.pixel;
}

void logger(const char* e) {
    fprintf(stderr,"\n\033[0;34m:: larry says : %s \033[0;m\n", e);
}

void init_desks(unsigned int ws) {
    desktops[ws].master_size = master_size;
    desktops[ws].nmaster = 0;
    desktops[ws].mode = mode;
    desktops[ws].growth = 0;
    desktops[ws].numwins = 0;
    desktops[ws].head = NULL;
    desktops[ws].current = NULL;
    desktops[ws].transient = NULL;
}

void setup() {
    // Install a signal
    sigchld(0);

    // Screen and root window
    screen = DefaultScreen(dis);
    root = RootWindow(dis,screen);

    // Initialize variables
    DESKTOPS = 4;
    msize = 55;
    ufalpha = 75;
    bdw = 2;
    attachaside = followmouse = 1;
    clicktofocus = mode = top_stack = 0;
    topbar = BAR_HEIGHT = bar_height = doinfo = 0;

    char *loc;
    loc = setlocale(LC_ALL, "");
    if (!loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX") || !XSupportsLocale())
        logger("LOCALE FAILED");
    // Read in RC_FILE
    sprintf(RC_FILE, "%s/.config/larrythewindowmanager/rc.conf", getenv("HOME"));
    sprintf(KEY_FILE, "%s/.config/larrythewindowmanager/key.conf", getenv("HOME"));
    set_defaults();
    read_rcfile();

    // Shortcuts
    grabkeys();

    // For exiting
    bool_quit = 0;

    // Master size
    master_size = (mode == 2) ? (sh*msize)/100 : (sw*msize)/100;

    // Set up all desktop
    unsigned int i;
    for(i=0;i<DESKTOPS;++i) {
        init_desks(i);
    }

    // Select first dekstop by default
    select_desktop(0);
    alphaatom = XInternAtom(dis, "_NET_WM_WINDOW_OPACITY", False);
    wm_delete_window = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    protos = XInternAtom(dis, "WM_PROTOCOLS", False);
    update_current();

    // To catch maprequest and destroynotify (if other wm running)
    XSelectInput(dis,root,SubstructureNotifyMask|SubstructureRedirectMask);
    XSetErrorHandler(xerror);
    logger("\033[0;32mWe're up and running!");
    update_info();
}

void sigchld(int unused) {
    if(signal(SIGCHLD, sigchld) == SIG_ERR) {
        logger("\033[0;31mCan't install SIGCHLD handler");
        exit(1);
        }
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(dis) close(ConnectionNumber(dis));
            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}

/* There's no way to check accesses to destroyed windows, thus those cases are ignored (especially on UnmapNotify's).  Other types of errors call Xlibs default error handler, which may call exit.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    logger("\033[0;31mBad Window Error!");
    return xerrorxlib(dis, ee); /* may call exit */
}

void start() {
    XEvent ev;

    while(!bool_quit && !XNextEvent(dis,&ev)) {
        if(events[ev.type])
            events[ev.type](&ev);
    }
}


int main() {
    // Open display
    if(!(dis = XOpenDisplay(NULL))) {
        logger("\033[0;31mCannot open display!");
        exit(1);
    }

    // Setup env
    setup();

    // Start wm
    start();

    // Close display
    XCloseDisplay(dis);

    exit(0);
}