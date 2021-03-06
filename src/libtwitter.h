/*
 * libtwitter.h - Xtwitter: twitter client for X
 * Copyright (C) 2008-2015 HAMANO Tsukasa <code@cuspy.org>
 * This software is released under the MIT License.
 * See LICENSE file for more details.
 */

#include <time.h>

#define TWITTER_BASE_URI "https://api.twitter.com"
#define TWITTER_API_PATH_FRIENDS_TIMELINE "/statuses/friends_timeline.xml"
#define TWITTER_API_PATH_HOME_TIMELINE "/statuses/home_timeline.xml"
#define TWITTER_API_PATH_UPDATE "/1.1/statuses/update.json"
#define TWITTER_API_ACCESS_TOKEN "/oauth/access_token"

#define TWITTER_USER_STREAM_URI "https://userstream.twitter.com/2/user.json"
#define TWITTER_PUBLIC_STREAM_URI "https://stream.twitter.com/1.1/statuses/sample.json"
#define TWITTER_SEARCH_URI "http://search.twitter.com/search.atom"

typedef struct{
    const char *created_at;
    const char *id;
    const char *text;
    const char *source;

    const char *user_id;
    const char *user_name;
    const char *user_screen_name;
    const char *user_profile_image_url;
    const struct twitter_status_t *rt;
}twitter_status_t;

typedef struct{
    const char *base_uri;
    const char *user;
    const char *pass;
    const char *source;
    const char *lang;
    const char *consumer_key;
    const char *consumer_secret;
    const char *token_key;
    const char *token_secret;
    const char *shortener;
    char res_dir[PATH_MAX];
    char images_dir[PATH_MAX];
    long long last_friends_timeline;
    int fetch_interval;
    int show_interval;
    int alignment;
    int debug;
    int quiet;
    int error;
    int (*popup)(void *, twitter_status_t *);
    char *search_word;
}twitter_t;

twitter_t *twitter_new();
void twitter_free(twitter_t *twitter);
int twitter_config(twitter_t *twitter);
int twitter_xauth(twitter_t *twitter);


//int twitter_fetch(twitter_t *twitter, const char *api_uri, GByteArray *buf);
int twitter_update(twitter_t *twitter, const char *status);
int twitter_count(const char *text);
int twitter_shorten(twitter_t *twitter, const char *text, char *shortentext);

twitter_status_t *twitter_status_new();
void twitter_status_free(twitter_status_t *status);

void twitter_unescape(char *dst, const char *src, size_t n);
void twitter_xmlescape(char *dst, const char *src, size_t n);

void twitter_status_print(twitter_status_t *status);
void twitter_status_dump(twitter_status_t *status);

int twitter_image_name(twitter_status_t *status, char *name);
int twitter_stat_image(twitter_t *twitter, twitter_status_t *status);
int twitter_fetch_image(twitter_t *twitter, const char *url, const char* path);
int twitter_resize_image(twitter_t *twitter, const char* path);

void twitter_user_stream(twitter_t *twitter);
void twitter_public_stream(twitter_t *twitter);

typedef struct{
    char *data;
    int len;
    int cap;
}buf_t;

buf_t *buf_new(size_t cap);
void buf_free(buf_t *buf);
