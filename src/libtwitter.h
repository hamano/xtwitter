/*
 * Xtwitter - libtwitter.h
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

#include <time.h>

#define TWITTER_BASE_URI "https://twitter.com"
#define TWITTER_API_PATH_FRIENDS_TIMELINE "/statuses/friends_timeline.xml"
#define TWITTER_API_PATH_UPDATE "/statuses/update.xml"

typedef struct{
    const char *base_uri;
    const char *user;
    const char *pass;
    const char *source;
    char res_dir[PATH_MAX];
    char images_dir[PATH_MAX];
    unsigned long last_friends_timeline;
    int fetch_interval;
    int show_interval;
    int alignment;
    int debug;
    int error;
}twitter_t;

typedef struct{
    const char *id;
    const char *screen_name;
    const char *profile_image_url;
}twitter_user_t;

typedef struct{
    const char *created_at;
    const char *id;
    const char *text;
    const char *source;
    const twitter_user_t *user;
}twitter_status_t;

twitter_t *twitter_new();
void twitter_free(twitter_t *twitter);
int twitter_config(twitter_t *twitter);

int twitter_fetch(twitter_t *twitter, const char *api_uri, GByteArray *buf);
int twitter_update(twitter_t *twitter, const char *status);
GList* twitter_friends_timeline(twitter_t *twitter);
GList* twitter_parse_statuses_node(xmlTextReaderPtr reader);
twitter_status_t* twitter_parse_status_node(xmlTextReaderPtr reader);
twitter_user_t* twitter_parse_user_node(xmlTextReaderPtr reader);

void twitter_statuses_free(GList *statuses);
void twitter_status_print(twitter_status_t *status);

int twitter_fetch_images(twitter_t *twitter, GList *statuses);
int twitter_fetch_image(twitter_t *twitter, const char *url, char* path);
