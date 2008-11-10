/*
 * Xtwitter - xtwitter.c: twitter client for X
 * Copyright (C) 2008 Tsukasa Hamano <code@cuspy.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <libxml/xmlreader.h>
#include <X11/Xlib.h>
#include <X11/Xlocale.h>

#include "libtwitter.h"

#define XTWITTER_WINDOW_WIDTH 500
#define XTWITTER_WINDOW_HEIGHT 50

Display *display;
Window window;
GC gc;
XFontSet text_fonts;
XFontSet user_fonts;
unsigned long color_black, color_white;
int window_x, window_y;

int xtwitter_xinit()
{
    Window root;
    int screen;

    int root_x, root_y;
    unsigned int root_width, root_height, root_border, root_depth;
    int missing_count;
    char** missing_list;
    char* def_string;

    if(!setlocale( LC_CTYPE, "")){
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
    root   = DefaultRootWindow(display);
    screen = DefaultScreen(display);
    color_white  = WhitePixel(display, screen);
    color_black  = BlackPixel(display, screen);

    XGetGeometry(display, RootWindow(display, 0), &root,
                 &root_x, &root_y, &root_width, &root_height,
                 &root_border, &root_depth);

    text_fonts = XCreateFontSet(display, "-*-*-medium-r-normal--16-*-*-*", 
                                &missing_list, &missing_count, &def_string);
    if(!text_fonts){
        printf( "error: XCreateFontSet\n" );
        return 1;
    }
    if(!missing_list){
        XFreeStringList(missing_list);
    }
    user_fonts = XCreateFontSet(display, "-*-*-medium-r-normal--9-*-*-*", 
                                &missing_list, &missing_count, &def_string);
    if(!user_fonts){
        printf( "error: XCreateFontSet\n" );
        return 1;
    }
    if(!missing_list){
        XFreeStringList(missing_list);
    }
    
    window_x = root_width - XTWITTER_WINDOW_WIDTH - 10;
    window_y = root_height - XTWITTER_WINDOW_HEIGHT - 10;
    return 0;
}

int xtwitter_show_status(twitter_t *twitter, twitter_status_t *status)
{
    XSetWindowAttributes attr;
    
    window = XCreateSimpleWindow(display, RootWindow(display, 0),
                                 window_x, window_y,
                                 XTWITTER_WINDOW_WIDTH, XTWITTER_WINDOW_HEIGHT,
                                 1, color_black, color_white);

    gc = XCreateGC(display, window, 0, NULL);
    XSetBackground(display, gc, color_white);
    XSetForeground(display, gc, color_black);
    attr.override_redirect=True;
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &attr);
    XMapWindow(display, window);

    XmbDrawString(display, window, text_fonts, gc, 50, 30,
                  status->text, strlen(status->text));
    XmbDrawString(display, window, user_fonts, gc, 5, 45,
                  status->user->screen_name,
                  strlen(status->user->screen_name));
    XFlush(display);
    sleep(twitter->show_interval);
    XDestroyWindow(display, window);
    XFlush(display);
    return 0;
}

void xtwitter_show_timeline(twitter_t *twitter, GList *statuses){
    twitter_status_t *status;
    statuses = g_list_last(statuses);
    if(!statuses){
        return;
    }
    do{
        status = statuses->data;
        if(twitter->debug)
            twitter_status_print(status);
        xtwitter_show_status(twitter, status);
    }while((statuses = g_list_previous(statuses)));
}

void xtwitter_loop()
{
    twitter_t *twitter = NULL;
    GList* timeline = NULL;
    twitter = twitter_new();
    twitter_config(twitter);

    if(twitter->debug){
        timeline = twitter_friends_timeline(twitter);
        twitter_statuses_free(timeline);
    }

    while(1){
        timeline = twitter_friends_timeline(twitter);
        if(twitter->debug)
            printf("timeline num: %d\n", g_list_length(timeline));

        twitter_fetch_images(twitter, timeline);
        xtwitter_show_timeline(twitter, timeline);
        twitter_statuses_free(timeline);
        timeline = NULL;
        sleep(twitter->fetch_interval);
    }
    twitter_free(twitter);
}

void xtwitter_update(const char *text)
{
    twitter_t *twitter = NULL;
    twitter = twitter_new();
    twitter_config(twitter);
    twitter_update(twitter, text);
    twitter_free(twitter);
}

int main(int argc, char *argv[]){
    int ret;
    int opt;

    while((opt = getopt(argc, argv, "du:")) != -1){
        switch(opt){
        case 'd':
            printf("option d\n");
            break;
        case 'u':
            fprintf(stdout, "updating...");
            fflush(stdout);
            xtwitter_update(optarg);
            fprintf(stdout, "done\n");
            return EXIT_SUCCESS;
        default:
            fprintf(stderr, "usage: \n");
            return EXIT_FAILURE;
        }
    }

    ret = xtwitter_xinit();
    if(ret){
        return EXIT_FAILURE;
    }
    xtwitter_loop();
    return EXIT_SUCCESS;
}
