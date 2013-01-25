/*
 * Xtwitter - xtwitter.c: twitter client for X
 * Copyright (C) 2008-2013 Tsukasa Hamano <code@cuspy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xlocale.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/AsciiText.h>


#include <Imlib2.h>
#include <regex.h>
#include <pthread.h>

#include "libtwitter.h"

#define XTWITTER_WINDOW_WIDTH  400
#define XTWITTER_WINDOW_HEIGHT 100

Display *display;
Window window;
GC gc;
Pixmap pixmap;
Atom atom_delete_window;
XFontSet text_fonts;
XFontSet user_fonts;
unsigned long color_black, color_white;

Imlib_Image *image;
regex_t id_regex;

int xtwitter_x_init()
{
    Window root;
    int screen;
    int root_x, root_y;
    unsigned int root_width, root_height, root_border, root_depth;
    int missing_count;
    char** missing_list;
    char* def_string;

    XInitThreads();

    if(!setlocale(LC_CTYPE, "")){
        fprintf(stderr, "error: setlocale()\n");
        return -1;
    }
    if (!XSupportsLocale()){
        fprintf(stderr, "error: XSupportsLocale()\n");
        return -1;
    }
    display = XOpenDisplay(NULL);
    if(!display){
        fprintf(stderr, "Can't open display\n");
        return -1;
    }
    root = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    color_white= WhitePixel(display, screen);
    color_black= BlackPixel(display, screen);

    XGetGeometry(display, RootWindow(display, 0), &root,
                 &root_x, &root_y, &root_width, &root_height,
                 &root_border, &root_depth);

    text_fonts = XCreateFontSet(display, "-*-*-medium-r-normal--12-*-*-*", 
                                &missing_list, &missing_count, &def_string);

    if(!text_fonts){
        printf( "error: XCreateFontSet\n" );
        return 1;
    }
    if(!missing_list){
        XFreeStringList(missing_list);
    }

    user_fonts = XCreateFontSet(display, "-*-*-medium-r-normal--12-*-*-*", 
                                &missing_list, &missing_count, &def_string);
    if(!user_fonts){
        printf( "error: XCreateFontSet\n" );
        return 1;
    }
    if(!missing_list){
        XFreeStringList(missing_list);
    }

    window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0,
                                 XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT,
                                 1, color_black, color_white);

    atom_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &atom_delete_window, 1);

    gc = XCreateGC(display, window, 0, NULL);
    XSetBackground(display, gc, color_white);
    XSetForeground(display, gc, color_black);

	imlib_context_set_display(display);
	imlib_context_set_visual(DefaultVisual(display, 0));
	imlib_context_set_colormap(DefaultColormap(display, 0));

    pixmap = XCreatePixmap(display, window,
                           XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT,
                           DefaultDepth(display, screen));

    // control under window manager
    XSetWindowAttributes attr;
    attr.override_redirect = False;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);

    // set title bar
    XTextProperty  prop;
    prop.value    = (unsigned char *)PACKAGE;
    prop.encoding = XA_STRING;
    prop.format = 8;
    prop.nitems = strlen(PACKAGE);
    XSetWMName(display, window, &prop);

    // fill pixmap by white
    XSetForeground(display, gc, color_white);
    XSetBackground(display, gc, color_black);
    XFillRectangle(display, pixmap, gc,
                   0, 0, XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT);

    XMapWindow(display, window);
    XFlush(display);
    return 0;
}

int utf8pos(const char *str, int width){
    int i=0;
    unsigned char c;
    while(str[i] && width > 1){
        c = (unsigned char)str[i];
        width--;
        if(c < 0x80){
            i++;
            continue;
        }
        width--;
        while(c & 0x80){
            c<<=1;
            i++;
        }
    }
    return i;
}

int xtwitter_x_popup(void *config, twitter_status_t *status)
{
    twitter_t *twitter = config;
    char image_name[PATH_MAX];
    char image_path[PATH_MAX];
    int pad_y;
    int text_line=0;
    int pos;
    const char *text = status->text;
    int i;

    /* notice: destructive conversion  */
    //xmlunescape((char*)status->text);

    while(*text){
        pos=utf8pos(text, 56);
        text+=pos;
        text_line++;
    }

    twitter_image_name(status, image_name);
    snprintf(image_path, PATH_MAX, "%s/%s", twitter->images_dir, image_name);

/*
    XClearArea(display, window, 0, 0,
               XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT, 0);
*/
    XSetForeground(display, gc, color_white);
    XSetBackground(display, gc, color_black);

    XFillRectangle(display, pixmap, gc,
                   0, 0, XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT);

    switch(text_line){
    case 1:
        pad_y = 60;
        break;
    case 2:
        pad_y = 56;
        break;
    case 3:
        pad_y = 50;
        break;
    case 4:
        pad_y = 42;
        break;
    case 5:
        pad_y = 34;
        break;
    default:
        pad_y = 34;
    }

    XSetForeground(display, gc, color_black);
    XSetBackground(display, gc, color_white);

    text = status->text;
    i=0;
    while(*text){
        pos=utf8pos(text, 56);
        XmbDrawString(display, pixmap, text_fonts, gc,
                      58, pad_y + 14 * i++, text, pos);
        text+=pos;
    }

    char *name = NULL;
    asprintf(&name, "@%s (%s)", status->user_screen_name, status->user_name);
    XmbDrawString(display, pixmap, user_fonts, gc, 5, 17,
                  name,
                  strlen(name));
    free(name);

    image = imlib_load_image(image_path);
    if(image){
		imlib_context_set_image(image);
		imlib_context_set_drawable(pixmap);
		imlib_render_image_on_drawable(5, 35);
    }

    XCopyArea(display, pixmap, window, gc,
              0, 0, XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT, 0, 0);
    //XRaiseWindow(display, window);
    //XFlush(display);
    return 0;
}

XtAppContext context;
Widget widget_shell, widget_tl_viewport, widget_tl_form;

static char *default_resources[] = {
    "xtwitter.title: xtwitter",
    "xtwitter.geometry: 400x600",
    "xtwitter*international: True",
    "xtwitter*fontSet: -*-*-medium-r-normal--12-*-*-*, 6x12",
    "xtwitter*inputMethod: xim",
    "xtwitter*preeditType: OverTheSpot,OffTheSpot,Root",
    NULL
};

int xtwitter_xaw_insert(twitter_t *twitter, twitter_status_t *status)
{
    static Widget status_paned = NULL;
    static Widget status_prev_paned = NULL;
    Widget status_lower_paned = NULL;
    Widget status_text, name_label, icon_label;

    Arg args[16];
    int argn = 0;
    int height, width;
    char image_name[PATH_MAX];
    char image_path[PATH_MAX];

    Pixmap *pixmap = NULL;

    twitter_image_name(status, image_name);
    snprintf(image_path, PATH_MAX, "%s/%s", twitter->images_dir, image_name);
    image = imlib_load_image(image_path);

    pixmap = XCreatePixmap(XtDisplay(widget_shell),
                           XtWindow(widget_shell),
                           48, 48,
                           DefaultDepth(XtDisplay(widget_shell), 0));
    if(image){
		imlib_context_set_image(image);
		imlib_context_set_drawable(pixmap);
		imlib_render_image_on_drawable(0, 0);
    }

    XtVaGetValues(widget_tl_form, XtNwidth, &width, NULL);
    width -= 20; // margine

    //XawFormDoLayout(widget_tl_form, False);

    argn = 0;
    XtSetArg(args[argn], XtNheight, (XtArgVal)100); argn++;
    XtSetArg(args[argn], XtNwidth, (XtArgVal)width); argn++;
    XtSetArg(args[argn], XtNresizeToPreferred, (XtArgVal)True); argn++;
    XtSetArg(args[argn], XtNallowResize, (XtArgVal)True); argn++;
    XtSetArg(args[argn], XtNtop, (XtArgVal)XawChainTop); argn++;

    status_prev_paned = status_paned;
    status_paned = XtCreateWidget("status_paned",
                                  panedWidgetClass,
                                  widget_tl_form, args, argn);
    if(status_prev_paned!=NULL){
        XtVaSetValues(status_prev_paned,
                      XtNfromVert, (XtArgVal)status_paned,
                      NULL);
    }
    XtManageChild(status_paned);


    char *name = NULL;
    asprintf(&name, "@%s (%s)", status->user_screen_name, status->user_name);
    argn = 0;
    XtSetArg(args[argn], XtNlabel, name); argn++;
    XtSetArg(args[argn], XtNshowGrip, (XtArgVal)False); argn++;
    XtSetArg(args[argn], XtNjustify, (XtArgVal)XtJustifyLeft); argn++;
    XtSetArg(args[argn], XtNresizeToPreferred, (XtArgVal)True); argn++;
    //XtSetArg(args[argn], XtNallowResize, (XtArgVal)True); argn++;
    name_label = XtCreateManagedWidget("name_label",
                                       labelWidgetClass,
                                       status_paned, args, argn);
    free(name);

    argn = 0;
    XtSetArg(args[argn], XtNorientation, (XtArgVal)XtorientHorizontal); argn++;
    status_lower_paned = XtCreateManagedWidget("status_lower_paned",
                                               panedWidgetClass,
                                               status_paned, args, argn);

    argn = 0;
    XtSetArg(args[argn], XtNshowGrip, (XtArgVal)False); argn++;
    XtSetArg(args[argn], XtNheight, (XtArgVal)58); argn++;
    XtSetArg(args[argn], XtNwidth, (XtArgVal)58); argn++;
    XtSetArg(args[argn], XtNmin, (XtArgVal)58); argn++;
    XtSetArg(args[argn], XtNmax, (XtArgVal)58); argn++;
    XtSetArg(args[argn], XtNresizeToPreferred, (XtArgVal)True); argn++;
    XtSetArg(args[argn], XtNbitmap, (XtArgVal)pixmap); argn++;
    icon_label = XtCreateManagedWidget("icon",
                                      labelWidgetClass,
                                      status_lower_paned, args, argn);


    argn = 0;
    XtSetArg(args[argn], XtNstring, (XtArgVal)status->text); argn++;
    XtSetArg(args[argn], XtNwrap, (XtArgVal)XawtextWrapLine); argn++;
    XtSetArg(args[argn], XtNdisplayCaret, (XtArgVal)False); argn++;
    status_text = XtCreateManagedWidget("status_text",
                                        asciiTextWidgetClass,
                                        status_lower_paned, args, argn);



    //XtVaGetValues(status_text, XtNheight, &height, XtNwidth, &width, NULL);
    //printf("width, height: %d, %d\n", width, height);


    //XawFormDoLayout(widget_tl_form, True);
    XFlush(XtDisplay(widget_shell));
}

int xtwitter_xaw_init(){
    Arg args[16];
    int argn = 0;

    XInitThreads();
    XtSetLanguageProc(NULL, NULL, NULL);
    widget_shell = XtAppInitialize(&context, PACKAGE, NULL, 0, &argn, NULL,
                                   default_resources, NULL, 0);

    argn = 0;
    XtSetArg(args[argn], XtNallowVert, (XtArgVal)True); argn++;
    XtSetArg(args[argn], XtNuseRight, (XtArgVal)True); argn++;
    XtSetArg(args[argn], XtNforceBars, (XtArgVal)True); argn++;
    widget_tl_viewport = XtCreateManagedWidget("wdget_tl_viewport",
                                               viewportWidgetClass,
                                               widget_shell,
                                               args, argn);

    argn = 0;
    XtSetArg(args[argn], XtNdefaultDistance, (XtArgVal)10); argn++;
    widget_tl_form = XtCreateManagedWidget("widget_tl_form",
                                           formWidgetClass,
                                           widget_tl_viewport, args, argn);

    XtRealizeWidget(widget_shell);
    XFlush(XtDisplay(widget_shell));

    imlib_context_set_display(XtDisplay(widget_shell));
    imlib_context_set_visual(DefaultVisual(XtDisplay(widget_shell), 0));
    imlib_context_set_colormap(DefaultColormap(XtDisplay(widget_shell), 0));

    return 0;
}

void xtwitter_update(twitter_t *twitter, const char *text)
{
    int count;

    count = twitter_count(text);
	if(twitter->debug >= 1){
		printf("count: %d\n", count);
	}
	if(count > 140){
		fprintf(stdout, "message length is too long.\n");
		fprintf(stdout, "count: %d\n", count);
		return;
	}
    fprintf(stdout, "updating...");
    fflush(stdout);
    twitter_update(twitter, text);
    fprintf(stdout, "done\n");
}

int xtwitter_count(twitter_t *twitter, const char *text)
{
    int count;
    char shortentext[PATH_MAX];
    int ret = 0;

    count = twitter_count(text);
    printf("text: %s\n", text);
    printf("count: %d\n", count);

    if(twitter->shortener){
        ret = twitter_shorten(twitter, text, shortentext);
    }
    printf("shortencount: %d\n", twitter_count(shortentext));
    printf("shortentext: %s\n", shortentext);
    return ret;
}

void xtwitter_update_stdin(twitter_t *twitter)
{
    char text[1024];
    fgets(text, 1024, stdin);
    xtwitter_update(twitter, text);
}

static void daemonize(void)
{
    pid_t pid, sid;

    if(getppid() == 1)
        return;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }else if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

void *xtwitter_stream_thread(void *arg){
    twitter_t *twitter = arg;

    //while(1) sleep(10);
    twitter_user_stream(twitter);
    //twitter_public_stream(twitter);

    return NULL;
}

int main(int argc, char *argv[]){
    int ret;
    int opt;
    int opt_debug = 0;
    int opt_search = 0;
    int opt_daemonize = 0;
    char opt_search_word[1024];
    char *opt_update = NULL;
    char *opt_count = NULL;
    char *opt_lang = NULL;

    twitter_t *twitter = NULL;

    while((opt = getopt(argc, argv, "ds:l:u:c:vD")) != -1){
        switch(opt){
        case 'd':
			opt_debug++;
            break;
        case 's':
			opt_search = 1;
			if(optarg[0] == '#'){
				snprintf(opt_search_word, 1024, "%%23%s", optarg + 1);
			}else{
				snprintf(opt_search_word, 1024, "%s", optarg);
			}
			break;
        case 'l':
            opt_lang = optarg;
            break;
        case 'u':
            opt_update = optarg;
            break;
        case 'c':
            opt_count = optarg;
            break;
        case 'v':
            printf("%s %s\n", PACKAGE, VERSION);
            return EXIT_SUCCESS;
        case 'D':
            opt_daemonize = 1;
            break;
        default:
            fprintf(stderr, "usage:\n");
            fprintf(stderr, "  %s\n", PACKAGE);
            fprintf(stderr, "  or\n");
            fprintf(stderr, "  %s -u \"update status\"\n", PACKAGE);
            return EXIT_FAILURE;
        }
    }

    twitter = twitter_new();
    ret = twitter_config(twitter);
    if(ret){
        fprintf(stderr, "config error.\n");
        return EXIT_FAILURE;
    }

    if(opt_debug){
        twitter->debug = opt_debug;
        printf("debug: %d\n", twitter->debug);
    }

    if(opt_lang){
        twitter->lang = opt_lang;
        printf("lang: %s\n", twitter->lang);
    }

    if(opt_count){
        xtwitter_count(twitter, opt_count);
        twitter_free(twitter);
        return EXIT_SUCCESS;
    }

    ret = twitter_xauth(twitter);
    if(ret){
        fprintf(stderr, "error: xAuth failed.\n");
        twitter_free(twitter);
        return EXIT_FAILURE;
    }

    if(opt_update){
        if(!strcmp(opt_update, "-")){
            xtwitter_update_stdin(twitter);
        }else{
            xtwitter_update(twitter, opt_update);
        }
        twitter_free(twitter);
        return EXIT_SUCCESS;
    }

    regcomp(&id_regex, twitter->user, REG_EXTENDED | REG_NOSUB);

    //ret = xtwitter_x_init();
    ret = xtwitter_xaw_init();
    if(ret){
        fprintf(stderr, "xtwitter: error at xtwitter_init_x()\n");
        return EXIT_FAILURE;
    }

    //twitter->popup = xtwitter_x_popup;
    twitter->popup = xtwitter_xaw_insert;
    pthread_t stream_thread;

    int status = pthread_create(&stream_thread,
                                NULL, xtwitter_stream_thread, twitter);
    if(status){
        fprintf(stderr, "xtwitter: error at pthread_create()\n");
        return EXIT_FAILURE;
    }

    if(opt_daemonize){
        daemonize();
    }

    XtAppMainLoop(context);
/*
    XEvent event;
    XSelectInput(display, window, ExposureMask);
    while(1){
        while(XPending(display)){
            XLockDisplay(display);
            XNextEvent(display, &event);
            XUnlockDisplay(display);
            switch(event.type){
            case Expose:
                //printf("event: Expose count=%d\n", event.xexpose.count);
                XCopyArea(display, pixmap, window, gc,
                          0, 0, XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT, 0, 0);
                //XFlush(display);
                break;
            case ClientMessage:
                if(event.xclient.data.l[0] == atom_delete_window){
                    goto exit;
                }
                break;
            }
        }
        usleep(100000);
    }
*/

exit:
    pthread_join(stream_thread, NULL);
    twitter_free(twitter);
    return EXIT_SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
