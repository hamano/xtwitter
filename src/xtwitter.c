/*
 * xtwitter.c - Xtwitter: twitter client for X
 * Copyright (c) 2008-2015 HAMANO Tsukasa <code@cuspy.org>
 * This software is released under the MIT License.
 * See LICENSE file for more details.
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

char *strbrkdup(const char *text, int width){
    int pos;
    char *ret;
    ret = malloc(strlen(text) * 2);
    if(!ret){
        return NULL;
    }
    *ret = '\0';
    while(*text){
        pos = utf8pos(text, width);
        strncat(ret, text, pos);
        strncat(ret, "\n", 1);
        text+=pos;
    }
    return ret;
}

int xtwitter_xaw_insert(void *ctx, twitter_status_t *status)
{
    twitter_t *twitter = ctx;
    static Widget status_paned = NULL;
    static Widget status_prev_paned = NULL;
    Widget status_lower_paned = NULL;

    int height, width;
    char image_name[PATH_MAX];
    char image_path[PATH_MAX];

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

    XtVaGetValues(widget_tl_form, XtNwidth, &width, XtNheight, &height, NULL);
    width -= 20; // margine

    //XawFormDoLayout(widget_tl_form, False);

    status_prev_paned = status_paned;
    status_paned = XtVaCreateWidget(
        "status_paned",
        panedWidgetClass,
        widget_tl_form,
        XtNheight, 100,
        XtNwidth, width,
        XtNresizeToPreferred, True,
        XtNallowResize, True,
        XtNtop, (XtArgVal)XawChainTop,
        NULL);

    if(status_prev_paned!=NULL){
        XtVaSetValues(status_prev_paned,
                      XtNfromVert, (XtArgVal)status_paned,
                      NULL);
    }
    XtManageChild(status_paned);

    char *name = NULL;
    twitter_status_t *rt = (twitter_status_t *)status->rt;
    //static int count;
    if(rt){
        asprintf(&name, "@%s Retweeted by @%s (%s)",
                 rt->user_screen_name,
                 status->user_screen_name, status->user_name);
    }else{
        asprintf(&name, "@%s (%s)",
                 status->user_screen_name, status->user_name);
    }

    /* name_label = */
    XtVaCreateManagedWidget(
        "name_label",
        labelWidgetClass,
        status_paned,
        XtNlabel, name,
        XtNshowGrip, False,
        XtNjustify, XtJustifyLeft,
        XtNresizeToPreferred, True,
        //XtNallowResize, True,
        NULL);
    free(name);

    status_lower_paned = XtVaCreateManagedWidget(
        "status_lower_paned",
        panedWidgetClass,
        status_paned,
        XtNorientation, (XtArgVal)XtorientHorizontal,
        NULL);

    /* icon_label = */
    XtVaCreateManagedWidget(
        "icon_label",
        labelWidgetClass,
        status_lower_paned,
        XtNheight, 58,
        XtNwidth, 58,
        XtNmin, 58,
        XtNmax, 58,
        //XtNresizeToPreferred, True,
        XtNbitmap, pixmap,
        XtNshowGrip, False,
        NULL);

    char *text = strbrkdup(status->text, 50);
    /* status_text = */
    XtVaCreateManagedWidget(
        "status_text",
        labelWidgetClass,
        status_lower_paned,
        XtNlabel, text,
        XtNshowGrip, False,
        XtNjustify, XtJustifyLeft,
        XtNresizeToPreferred, True,
        //XtNallowResize, True,
        NULL);

/*
    status_text = XtVaCreateManagedWidget(
        "status_text",
        asciiTextWidgetClass,
        status_lower_paned,
        //status_paned,
        XtNstring, (XtArgVal)status->text,
        XtNeditType, XawtextRead,
        XtNwrap, XawtextWrapWord,
        XtNdisplayCaret, False,
        //XtNfromVert, name_label,
        //XtNwidth, width - 10,
        //XtNheight, 65,
        NULL);
*/

    //XtVaGetValues(status_text, XtNheight, &height, XtNwidth, &width, NULL);
    //printf("width, height: %d, %d\n", width, height);
    //XawFormDoLayout(widget_tl_form, True);
    XFlush(XtDisplay(widget_shell));
    return 0;
}

int xtwitter_xaw_init(){
    int argc = 0;
    XInitThreads();
    XtSetLanguageProc(NULL, NULL, NULL);
    widget_shell = XtAppInitialize(&context, PACKAGE, NULL, 0, &argc, NULL,
                                   default_resources, NULL, 0);

    widget_tl_viewport = XtVaCreateManagedWidget(
        "wdget_tl_viewport",
        viewportWidgetClass,
        widget_shell,
        XtNallowVert, True,
        XtNuseRight, True,
        XtNforceBars, True,
        NULL);

    widget_tl_form = XtVaCreateManagedWidget(
        "widget_tl_form",
        formWidgetClass,
        widget_tl_viewport,
        XtNdefaultDistance, 10,
        NULL);

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

    if(twitter->search_word){
        twitter_public_stream(twitter);
    }else{
        twitter_user_stream(twitter);
    }

    return NULL;
}

void xtwitter_x_loop(){
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
                    return;
                }
                break;
            }
        }
        usleep(100000);
    }
}

void xtwitter_xaw_loop(){
    //XtAppMainLoop(context);
    XEvent event;
    while(1){
        XtAppNextEvent(context, &event);
        XtDispatchEvent(&event);
    }
}

int main(int argc, char *argv[]){
    int ret;
    int opt;
    int opt_debug = 0;
    int opt_search = 0;
    int opt_daemonize = 0;
    char *opt_search_word = NULL;
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
				asprintf(&opt_search_word, "%%23%s", optarg + 1);
			}else{
				asprintf(&opt_search_word, "%s", optarg);
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
    if(opt_search){
        twitter->search_word = opt_search_word;
        printf("search_word: %s\n", twitter->search_word);
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
    pthread_t thread;
    int thread_status;
    thread_status = pthread_create(&thread, NULL,
                                   xtwitter_stream_thread, twitter);
    if(thread_status){
        fprintf(stderr, "xtwitter: error at pthread_create()\n");
        return EXIT_FAILURE;
    }

    if(opt_daemonize){
        daemonize();
    }

    xtwitter_xaw_loop();

    pthread_join(thread, NULL);
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
