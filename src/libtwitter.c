/*
 * Xtwitter - libtwitter.c
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>
#include <curl/curl.h>
#include <libxml/xmlreader.h>
#include <Imlib2.h>
#include "libtwitter.h"

twitter_t* twitter_new()
{
    twitter_t *twitter;
    const char *home;

    home = getenv("HOME");
    if(!home)
        return NULL;

    twitter = (twitter_t *)malloc(sizeof(twitter_t));
    twitter->base_uri = TWITTER_BASE_URI;
    twitter->user = NULL;
    twitter->pass = NULL;
    twitter->source = "Xtwitter";
    twitter->last_friends_timeline = 1;
    twitter->fetch_interval = 60;
    twitter->show_interval = 5;
    twitter->alignment = 2;
    twitter->debug = 0;
    snprintf(twitter->res_dir, PATH_MAX, "%s/.xtwitter", home);
    snprintf(twitter->images_dir, PATH_MAX, "%s/.xtwitter/images", home);

    return twitter;
}

void twitter_free(twitter_t *twitter)
{
    if(twitter->user){
        free((char*)(twitter->user));
    }
    if(twitter->pass){
        free((char*)(twitter->pass));
    }
    free(twitter);
    return;
}

int twitter_config(twitter_t *twitter)
{
    const char *home;
    char config_dir[PATH_MAX];
    char config_file[PATH_MAX];
    FILE *fp;
    struct stat st;
    char line[256];
    char *key;
    char *value;

    home = getenv("HOME");
    if(!home){
        return -1;
    }
    snprintf(config_dir, PATH_MAX, "%s/.xtwitter/", home);
    snprintf(config_file, PATH_MAX, "%s/.xtwitter/config", home);

    fp = fopen(config_file, "r");
    if(!fp){
        fprintf(stderr, "config open error\n");
        if(stat(config_dir, &st)){
            mkdir(config_dir, 0755);
        }
        if(stat(config_file, &st)){
            fp = fopen(config_file, "w");
            fprintf(fp, "user=\npass=\n");
            fclose(fp);
        }
        return -1;
    }
    while ((fgets(line, 256 - 1, fp)) != NULL) {
        key=line;
        if(strlen(line) < 1) continue;
        if(line[0] == '#') continue;
        value = strchr(line, '=');
        if(!value) continue;
        *value = '\0';
        value++;
        value[strlen(value) - 1] = '\0';
        if(!strcmp(key, "debug")){
            twitter->debug = atoi(value);
        }
        if(!strcmp(key, "user")){
            twitter->user = strdup(value);
        }
        if(!strcmp(key, "pass")){
            twitter->pass = strdup(value);
        }
        if(!strcmp(key, "fetch_interval")){
            twitter->fetch_interval = atoi(value);
            if(twitter->fetch_interval < 0){
                fprintf(stderr, "config read error:\n");
            }
        }
        if(!strcmp(key, "show_interval")){
            twitter->show_interval = atoi(value);
            if(twitter->show_interval < 0){
                fprintf(stderr, "config read error:\n");
            }
        }
        if(!strcmp(key, "alignment")){
            if(!strcmp(value, "top_left")){
                twitter->alignment = 0;
            }else if(!strcmp(value, "top_right")){
                twitter->alignment = 1;
            }else if(!strcmp(value, "bottom_left")){
                twitter->alignment = 2;
            }else if(!strcmp(value, "bottom_right")){
                twitter->alignment = 3;
            }else{
                twitter->alignment = 2;
            }
        }
    }
    fclose(fp);
    return 0;
}

static size_t twitter_curl_write_cb(void *ptr, size_t size, size_t nmemb,
                                    void *data)
{
    size_t realsize = size * nmemb;
    g_byte_array_append((GByteArray *)data, (guint8*)ptr, realsize);
    return realsize;
}

int twitter_fetch(twitter_t *twitter, const char *apiuri, GByteArray *buf)
{
    CURL *curl;
    CURLcode code;
    long res;
    char userpass[256];
    snprintf(userpass, 256, "%s:%s", twitter->user, twitter->pass);
    curl = curl_easy_init();
    if(!curl){
        fprintf(stderr, "error: curl_easy_init()\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, apiuri);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, TRUE);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buf);

    code = curl_easy_perform(curl);
    if(code){
        fprintf(stderr, "error: %s\n", curl_easy_strerror(code));
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
    if(res != 200){
        fprintf(stderr, "error respose code: %ld\n", res);
        return res;
    }

    curl_easy_cleanup(curl);
    return 0;
}

int twitter_update(twitter_t *twitter, const char *status)
{
    CURL *curl;
    CURLcode code;
    long res;
    char api_uri[PATH_MAX];
    struct curl_httppost *formpost=NULL;
    struct curl_httppost *lastptr=NULL;
    struct curl_slist *headers=NULL;
    GByteArray *buf;
    char userpass[256];

    snprintf(userpass, 256, "%s:%s", twitter->user, twitter->pass);
    buf = g_byte_array_new();
    curl = curl_easy_init();
    if(!curl) {
        printf("error: curl_easy_init()\n");
        return -1;
    }
    snprintf(api_uri, PATH_MAX, "%s%s",
             twitter->base_uri, TWITTER_API_PATH_UPDATE);
    if(twitter->debug >= 2)
        printf("api_uri: %s\n", api_uri);

    headers = curl_slist_append(headers, "Expect:");

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "status",
                 CURLFORM_COPYCONTENTS, status,
                 CURLFORM_END);

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "source",
                 CURLFORM_COPYCONTENTS, twitter->source,
                 CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_URL, api_uri);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, TRUE);
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buf);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    code = curl_easy_perform(curl);
    if(code){
        printf("error: %s\n", curl_easy_strerror(code));
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
    if(res != 200){
        printf("error respose code: %ld\n", res);
        if(twitter->debug > 2){
            fwrite(buf->data, 1, buf->len, stderr);
            fprintf(stderr, "\n");
        }
        return res;
    }
    curl_easy_cleanup(curl);
    curl_formfree(formpost);
    curl_slist_free_all(headers);
    return 0;
}

GList* twitter_friends_timeline(twitter_t *twitter)
{
    int ret;
    GList *timeline = NULL;
    GByteArray *buf;
    xmlTextReaderPtr reader;
    char api_uri[PATH_MAX];
    twitter_status_t *status;

    snprintf(api_uri, PATH_MAX, "%s%s?since_id=%llu",
             twitter->base_uri, TWITTER_API_PATH_FRIENDS_TIMELINE,
             twitter->last_friends_timeline);
    if(twitter->debug > 1)
        printf("api_uri: %s\n", api_uri);

    buf = g_byte_array_new();

    ret = twitter_fetch(twitter, api_uri, buf);
    if(ret){
        printf("ERROR: twitter_fetch()\n");
        return NULL;
    }
    reader = xmlReaderForMemory((const char *)buf->data, buf->len,
                                NULL, NULL, 0);
    timeline = twitter_parse_statuses_node(reader);
    xmlFreeTextReader(reader);
    g_byte_array_free (buf, TRUE);
//    xmlMemoryDump();

    if(timeline){
        status = timeline->data;
        twitter->last_friends_timeline = atoll(status->id);
    }

    return timeline;
}

GList* twitter_parse_statuses_node(xmlTextReaderPtr reader)
{
    int ret;
    xmlElementType type;
    xmlChar *name;
    GList* statuses = NULL;
    twitter_status_t *status;

    do{
        ret = xmlTextReaderRead(reader);
        type = xmlTextReaderNodeType(reader);
        if(type == XML_READER_TYPE_ELEMENT) {
            name = xmlTextReaderName(reader);
            if(!xmlStrcmp(name, (xmlChar *)"status")) {
                status = twitter_parse_status_node(reader);
                if(status){
                    statuses = g_list_append(statuses, status);
                }
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return statuses;
}

twitter_status_t* twitter_parse_status_node(xmlTextReaderPtr reader){
    int ret;
    xmlElementType type;
    xmlChar *name;
    twitter_status_t *status;
    status = (twitter_status_t *)malloc(sizeof(twitter_status_t));
    memset(status, 0, sizeof(twitter_status_t));

    do{
        ret = xmlTextReaderRead(reader);
        type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_ELEMENT){
            name = xmlTextReaderName(reader);
            if (!xmlStrcmp(name, (xmlChar *)"created_at")){
                ret = xmlTextReaderRead(reader);
                status->created_at = (const char *)xmlTextReaderValue(reader);
            }else if (!xmlStrcmp(name, (xmlChar *)"id")){
                ret = xmlTextReaderRead(reader);
                status->id = (const char *)xmlTextReaderValue(reader);
            }else if (!xmlStrcmp(name, (xmlChar *)"text")){
                ret = xmlTextReaderRead(reader);
                status->text = (const char *)xmlTextReaderValue(reader);
            }else if (!xmlStrcmp(name, (xmlChar *)"user")){
                status->user = twitter_parse_user_node(reader);
            }
            xmlFree(name);
        } else if (type == XML_READER_TYPE_END_ELEMENT){
            name = xmlTextReaderName(reader);
            if (!xmlStrcmp(name, (xmlChar *)"status")){
                xmlFree(name);
                break;
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return status;
}

twitter_user_t* twitter_parse_user_node(xmlTextReaderPtr reader){
    int ret;
    xmlElementType type;
    xmlChar *name;
    twitter_user_t *user;

    user = (twitter_user_t *)malloc(sizeof(twitter_user_t));
    memset(user, 0, sizeof(twitter_user_t));
    do{
        ret = xmlTextReaderRead(reader);
        type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_ELEMENT){
            name = xmlTextReaderName(reader);
            if(!xmlStrcmp(name, (xmlChar *)"id")){
                ret = xmlTextReaderRead(reader);
                user->id = (const char *)xmlTextReaderValue(reader);
            }else if(!xmlStrcmp(name, (xmlChar *)"screen_name")){
                ret = xmlTextReaderRead(reader);
                user->screen_name = (const char *)xmlTextReaderValue(reader);
            }else if(!xmlStrcmp(name, (xmlChar *)"profile_image_url")){
                ret = xmlTextReaderRead(reader);
                user->profile_image_url =
                    (const char *)xmlTextReaderValue(reader);
            }
            xmlFree(name);
        }else if(type == XML_READER_TYPE_END_ELEMENT){
            name = xmlTextReaderName(reader);
            if(!xmlStrcmp(name, (xmlChar *)"user")){
                xmlFree(name);
                break;
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return user;
}

void twitter_status_print(twitter_status_t *status){
    printf("@%s: %s\n", status->user->screen_name, status->text);
}

void twitter_status_dump(twitter_status_t *status){
    printf("[%s] @%s: %s\n", status->id,
           status->user->screen_name, status->text);
}

void twitter_statuses_free(GList *statuses){
    GList *l = statuses;
    twitter_status_t *status;
    if(!statuses){
        return;
    }
    do{
        status = l->data;
        if(!status){
            continue;
        }
        free((void*)(status->created_at));
        free((void*)(status->id));
        free((void*)(status->text));
        free((void*)(status->source));
        if(status->user){
            free((void*)(status->user->id));
            free((void*)(status->user->screen_name));
            free((void*)(status->user->profile_image_url));
            free((void*)(status->user));
        }
        free(status);
    }while((l = g_list_next(l)));
    g_list_free(statuses);
}

int twitter_image_name(twitter_status_t *status, char *name){
    size_t i;
    i = strlen(status->user->profile_image_url);
    while(i--)
        if(status->user->profile_image_url[i] == '/')
            break;
    while(i--)
        if(status->user->profile_image_url[i] == '/')
            break;
    i++;
    strncpy(name, status->user->profile_image_url + i, PATH_MAX - 1);
    i = strlen(name);
    while(i--)
        if(name[i] == '/')
            name[i] = '_';
    return 0;
}

int twitter_fetch_images(twitter_t *twitter, GList *statuses){
    int ret;
    twitter_status_t *status;
    const char *url;
    char name[PATH_MAX];
    char path[PATH_MAX];
    struct stat st;

    statuses = g_list_last(statuses);
    if(!statuses){
        return 0;
    }
    ret = mkdir(twitter->images_dir, 0755);
    if(ret && errno != EEXIST){
        fprintf(stderr, "can't create directory.\n");
        return -1;
    }

    do{
        status = statuses->data;
        twitter_image_name(status, name);
        url = status->user->profile_image_url;
        snprintf(path, PATH_MAX, "%s/%s", twitter->images_dir, name);
        ret = stat(path, &st);
        if(ret){
            twitter_fetch_image(twitter, url, path);
        }
    }while((statuses = g_list_previous(statuses)));
    return 0;
}

static size_t twitter_curl_file_cb(void *ptr, size_t size, size_t nmemb,
                                   void *data)
{
    size_t realsize = size * nmemb;
    fwrite(ptr, size, nmemb, (FILE*)data);
    return realsize;
}

int twitter_fetch_image(twitter_t *twitter, const char *url, char* path){
    CURL *curl;
    CURLcode code;
    long res;
    FILE *fp;
    int i;
    char *esc;
    char escaped_url[PATH_MAX];
	int ret;

    if(twitter->debug >= 2){
        printf("fetch image: %s\n", url);
    }
    fp = fopen(path, "w");
    if(!fp){
        fprintf(stderr, "error: can't openfile %s\n", path);
        return -1;
    }

    curl = curl_easy_init();
    if(!curl){
        fprintf(stderr, "error: curl_easy_init()\n");
        return -1;
    }

    /* url escape */
    i = strlen(url);
    while(i-- && url[i] != '/');
    esc = curl_easy_escape(curl, url + i + 1, 0);
    strncpy(escaped_url, url, PATH_MAX - 1);
    strncpy(escaped_url + i + 1, esc, PATH_MAX - i);
    curl_free(esc);

    curl_easy_setopt(curl, CURLOPT_URL, escaped_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fp);

    code = curl_easy_perform(curl);
    if(code){
        fprintf(stderr, "error: %s\n", curl_easy_strerror(code));
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
    if(res != 200){
        fprintf(stderr, "error respose code: %ld\n", res);
        return res;
    }
    fclose(fp);

    //ret = twitter_resize_image();

    return 0;
}

int twitter_resize_image(twitter_t *twitter, char* path){
    if(twitter->debug >= 2){
        printf("resize image: %s\n", path);
    }
}
