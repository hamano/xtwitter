/*
 * Xtwitter - libtwitter.c
 * Copyright (C) 2008-2012 Tsukasa Hamano <code@cuspy.org>
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
#include <oauth.h>
#include <regex.h>
#include <json/json.h>
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
    twitter->lang = NULL;
    twitter->last_friends_timeline = -1;
    twitter->fetch_interval = 30;
    twitter->show_interval = 5;
    twitter->alignment = 2;
    twitter->debug = 0;
    twitter->quiet = 0;
    twitter->consumer_key = "9poct2ZKf927Sjb3ZprdQ";
    twitter->consumer_secret = "xYGTxLldXFcm20hUSfztZtaHSViEr6xOQLJVAc5RI";
    twitter->token_key = NULL;
    twitter->token_secret = NULL;
    snprintf(twitter->res_dir, PATH_MAX, "%s/.xtwitter", home);
    snprintf(twitter->images_dir, PATH_MAX, "%s/.xtwitter/images", home);

    twitter->shortener = NULL;
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
    if(twitter->token_key){
        free((char *)twitter->token_key);
    }
    if(twitter->token_secret){
        free((char *)twitter->token_secret);
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
        if(!strcmp(key, "quiet")){
            twitter->quiet = atoi(value);
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
        if(!strcmp(key, "token_key")){
            twitter->token_key = strdup(value);
        }
        if(!strcmp(key, "token_secret")){
            twitter->token_secret = strdup(value);
        }
        if(!strcmp(key, "shortener")){
            twitter->shortener = strdup(value);
        }
    }
    fclose(fp);
    return 0;
}

int twitter_xauth(twitter_t *twitter)
{
    char access_token_uri[PATH_MAX];
    char parm_user[256];
    char parm_pass[256];
    int argc = 4;
    char **argv = (char **)malloc(sizeof(char*) * 4);
    char *req_url = NULL;
    char *postargs = NULL;
    char *res = NULL;
    char *t;

    snprintf(access_token_uri, PATH_MAX, "%s%s",
             twitter->base_uri, TWITTER_API_ACCESS_TOKEN);

    argv[0] = access_token_uri;
    argv[1] = strdup("x_auth_mode=client_auth");
    snprintf(parm_user, sizeof(parm_user), "x_auth_username=%s",
             twitter->user);
    snprintf(parm_pass, sizeof(parm_pass), "x_auth_password=%s",
             twitter->pass);
    argv[2] = parm_user;
    argv[3] = parm_pass;

    req_url = oauth_sign_array2(&argc, &argv, &postargs, OA_HMAC, NULL,
                                twitter->consumer_key,
                                twitter->consumer_secret,
                                NULL, NULL);

    res = oauth_http_post(req_url, postargs);
    if(!res){
        return -1;
    }

    t = strtok(res, "&");
    do{
        if(!strncmp(t, "oauth_token=", 12)){
            twitter->token_key = strdup(t + 12);
        }
        if(!strncmp(t, "oauth_token_secret=", 19)){
            twitter->token_secret = strdup(t + 19);
        }
        t = strtok(NULL, "&");
    }while(t != NULL);
    free(res);
    free(argv);
    if(twitter->token_key && twitter->token_secret){
        return 0;
    }else{
        return -1;
    }
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
    curl = curl_easy_init();
    if(!curl){
        fprintf(stderr, "error: curl_easy_init()\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, apiuri);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, TRUE);
    /* 2010-08-31 no need basic auth
      curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
    */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buf);
    // 2010-07-20 bug
    curl_easy_setopt(curl, CURLOPT_IGNORE_CONTENT_LENGTH, 1);

    code = curl_easy_perform(curl);
    if(code){
        fprintf(stderr, "error: %s\n", curl_easy_strerror(code));
        return -1;
    }
    if(twitter->debug >= 3){
        fwrite(buf->data, 1, buf->len, stderr);
        fprintf(stderr, "\n");
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
    if(res != 200){
        fprintf(stderr, "error respose code: %ld\n", res);
        return res;
    }

    curl_easy_cleanup(curl);
    return 0;
}

static size_t twitter_curl_stream_cb(void *ptr, size_t size, size_t nmemb,
                                     void *data)
{
    json_tokener *tokener;
    twitter_t *twitter = data;
    size_t realsize = size * nmemb;
    json_object *obj_root = NULL;
    json_object *obj_user = NULL;
    json_object *obj_tmp = NULL;
    twitter_status_t *status = NULL;
    twitter_user_t *user = NULL;

    if(realsize <= 2){
        return realsize;
    }
    tokener = json_tokener_new();
    obj_root = json_tokener_parse_ex(tokener, ptr, realsize);
    json_tokener_free(tokener);
    if (is_error(obj_root)){
        fprintf(stderr, "parse error: ptr=%s\n", ptr);
        json_object_put(obj_root);
        return realsize;
    }
    obj_tmp = json_object_object_get(obj_root, "friends");
    if(obj_tmp){
        printf("watching %d friends.\n", json_object_array_length(obj_tmp));
        json_object_put(obj_root);
        return realsize;
    }
    obj_user = json_object_object_get(obj_root, "user");
    if(!obj_user){
        fprintf(stderr, "not found user object\n");
        fprintf(stderr, "%s\n", json_object_to_json_string(obj_root));
        json_object_put(obj_root);
        return realsize;
    }

    status = (twitter_status_t *)malloc(sizeof(twitter_status_t));
    memset(status, 0, sizeof(twitter_status_t));
    user = (twitter_user_t *)malloc(sizeof(twitter_user_t));
    memset(user, 0, sizeof(twitter_user_t));
    status->user = user;

    obj_tmp = json_object_object_get(obj_root, "created_at");
    status->created_at = json_object_get_string(obj_tmp);
    obj_tmp = json_object_object_get(obj_root, "id_str");
    status->id = json_object_get_string(obj_tmp);
    obj_tmp = json_object_object_get(obj_root, "text");
    status->text = json_object_get_string(obj_tmp);
    obj_tmp = json_object_object_get(obj_root, "source");
    status->source = json_object_get_string(obj_tmp);

    obj_tmp = json_object_object_get(obj_user, "id");
    user->id = json_object_get_string(obj_tmp);
    obj_tmp = json_object_object_get(obj_user, "screen_name");
    user->screen_name = json_object_get_string(obj_tmp);
    obj_tmp = json_object_object_get(obj_user, "profile_image_url");
    user->profile_image_url = json_object_get_string(obj_tmp);

    if(twitter->debug > 1){
        twitter_status_dump(status);
    }else if(twitter->quiet == 0){
        twitter_status_print(status);
    }
    twitter->popup(twitter, status);

    json_object_put(obj_root);
    free(status);
    free(user);
    return realsize;
}

int twitter_user_stream_read(twitter_t *twitter, const char *apiuri)
{
    CURL *curl;
    CURLcode code;
    long res;
    curl = curl_easy_init();
    if(!curl){
        fprintf(stderr, "error: curl_easy_init()\n");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, apiuri);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, TRUE);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_stream_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)twitter);
    // 2010-07-20 bug
    //curl_easy_setopt(curl, CURLOPT_IGNORE_CONTENT_LENGTH, 1);

    code = curl_easy_perform(curl);
    if(code){
        fprintf(stderr, "error: %s\n", curl_easy_strerror(code));
        return -1;
    }
/*
    if(twitter->debug >= 3){
        fwrite(buf->data, 1, buf->len, stderr);
        fprintf(stderr, "\n");
    }
*/
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
    char *req_uri;

    buf = g_byte_array_new();
    curl = curl_easy_init();
    if(!curl) {
        fprintf(stderr, "error: curl_easy_init()\n");
        return -1;
    }
    snprintf(api_uri, PATH_MAX, "%s%s",
             twitter->base_uri, TWITTER_API_PATH_UPDATE);

    req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "POST",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug >= 2){
        printf("req_uri: %s\n", req_uri);
    }

    headers = curl_slist_append(headers, "Expect:");

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "status",
                 CURLFORM_COPYCONTENTS, status,
                 CURLFORM_END);

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "source",
                 CURLFORM_COPYCONTENTS, twitter->source,
                 CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_URL, req_uri);
    free(req_uri);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, TRUE);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, TRUE);
    //curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    //curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);
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

GList* twitter_home_timeline(twitter_t *twitter)
{
    int ret;
    GList *timeline = NULL;
    GByteArray *buf;
    xmlTextReaderPtr reader;
    char api_uri[PATH_MAX];
    twitter_status_t *status;

    snprintf(api_uri, PATH_MAX, "%s%s?since_id=%lld",
             twitter->base_uri, TWITTER_API_PATH_HOME_TIMELINE,
             twitter->last_friends_timeline);
    char *req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "GET",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug > 1){
        printf("req_uri: %s\n", req_uri);
    }

    buf = g_byte_array_new();
    ret = twitter_fetch(twitter, req_uri, buf);
    if(ret){
        printf("ERROR: twitter_fetch()\n");
        free(req_uri);
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
    free(req_uri);
    return timeline;
}

GList* twitter_friends_timeline(twitter_t *twitter)
{
    int ret;
    GList *timeline = NULL;
    GByteArray *buf;
    xmlTextReaderPtr reader;
    char api_uri[PATH_MAX];
    twitter_status_t *status;

    snprintf(api_uri, PATH_MAX, "%s%s?since_id=%lld",
             twitter->base_uri, TWITTER_API_PATH_FRIENDS_TIMELINE,
             twitter->last_friends_timeline);
    char *req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "GET",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug >= 2){
        //printf("api_uri: %s\n", api_uri);
        printf("req_uri: %s\n", req_uri);
    }

    buf = g_byte_array_new();
    ret = twitter_fetch(twitter, req_uri, buf);
    if(ret){
        printf("ERROR: twitter_fetch()\n");
        free(req_uri);
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
    free(req_uri);
    return timeline;
}

void twitter_user_stream(twitter_t *twitter)
{
    char api_uri[PATH_MAX];

    snprintf(api_uri, PATH_MAX, "%s", TWITTER_STREAM_URI);
    char *req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "GET",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug >= 2){
        printf("req_uri: %s\n", req_uri);
    }

    twitter_user_stream_read(twitter, req_uri);
}

GList* twitter_parse_statuses_node(xmlTextReaderPtr reader)
{
    xmlElementType type;
    xmlChar *name;
    GList* statuses = NULL;
    twitter_status_t *status;

    while(xmlTextReaderRead(reader) == 1){
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
    }
    return statuses;
}

twitter_status_t* twitter_parse_status_node(xmlTextReaderPtr reader){
    int ret;
    xmlElementType type;
    xmlChar *name;
    int depth;
    twitter_status_t *status;
    status = (twitter_status_t *)malloc(sizeof(twitter_status_t));
    memset(status, 0, sizeof(twitter_status_t));

    while(xmlTextReaderRead(reader) == 1){
        type = xmlTextReaderNodeType(reader);
        depth = xmlTextReaderDepth(reader);
        if(depth > 2){
            continue;
        }
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
    }
    return status;
}

twitter_user_t* twitter_parse_user_node(xmlTextReaderPtr reader){
    int ret;
    xmlElementType type;
    xmlChar *name;
    twitter_user_t *user;

    user = (twitter_user_t *)malloc(sizeof(twitter_user_t));
    memset(user, 0, sizeof(twitter_user_t));
    while(xmlTextReaderRead(reader) == 1){
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
    }
    return user;
}

void twitter_status_print(twitter_status_t *status){
    printf("@%s: %s\n", status->user->screen_name, status->text);
}

void twitter_status_dump(twitter_status_t *status){
    printf("[%s] @%s: %s\n", status->id,
           status->user->screen_name, status->text);
}

void twitter_status_free(twitter_status_t *status){
    if(status->created_at) free((void*)(status->created_at));
    if(status->id) free((void*)(status->id));
    if(status->text) free((void*)(status->text));
    if(status->source) free((void*)(status->source));
    if(status->user){
        if(status->user->id) free((void*)(status->user->id));
        if(status->user->screen_name) free((void*)(status->user->screen_name));
        if(status->user->profile_image_url)
            free((void*)(status->user->profile_image_url));
        if(status->user) free((void*)status->user);
    }
    free(status);
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
        twitter_status_free(status);
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
            twitter_resize_image(twitter, path);
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

int twitter_fetch_image(twitter_t *twitter, const char *url, const char* path){
    CURL *curl;
    CURLcode code;
    long res;
    FILE *fp;
    int i;
    char *esc;
    char escaped_url[PATH_MAX];

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

    return 0;
}

int twitter_resize_image(twitter_t *twitter, const char* path)
{
    Imlib_Image *image;
    Imlib_Image *scaled_image;
    int width, height;

    image = imlib_load_image(path);
    if(!image){
        return -1;
    }
    imlib_context_set_image(image);
    width = imlib_image_get_width();
    height = imlib_image_get_width();
    if(width > 48 || height > 48){
        if(twitter->debug >= 1){
            printf("resizing image due to too large(%d, %d)\n", width, height);
        }
        scaled_image = imlib_create_cropped_scaled_image(0, 0,
                                                         width, height,
                                                         48, 48);
        if(!scaled_image){
            printf("error\n");
            return -1;
        }
        imlib_context_set_image(scaled_image);
        imlib_save_image(path);
        imlib_free_image();
        imlib_context_set_image(image);
        if(twitter->debug >= 1){
            printf("resized image %s\n", path);
        }
    }
    imlib_free_image();
    return 0;
}

twitter_user_t* twitter_parse_author_node(xmlTextReaderPtr reader){
    int ret;
    xmlElementType type;
    xmlChar *name;
    twitter_user_t *user;
    char *name_tmp;

    user = (twitter_user_t *)malloc(sizeof(twitter_user_t));
    memset(user, 0, sizeof(twitter_user_t));
    do{
        ret = xmlTextReaderRead(reader);
        type = xmlTextReaderNodeType(reader);
        if (type == XML_READER_TYPE_ELEMENT){
            name = xmlTextReaderName(reader);
            if(!xmlStrcmp(name, (xmlChar *)"name")){
                ret = xmlTextReaderRead(reader);
                user->id = strdup("");
                user->screen_name = (const char *)xmlTextReaderValue(reader);
                name_tmp = strchr(user->screen_name, ' ');
                if(*name_tmp){
                    *name_tmp = '\0';
                }
                user->profile_image_url = strdup("");
            }
            xmlFree(name);
        }else if(type == XML_READER_TYPE_END_ELEMENT){
            name = xmlTextReaderName(reader);
            if(!xmlStrcmp(name, (xmlChar *)"author")){
                xmlFree(name);
                break;
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return user;
}

twitter_status_t* twitter_parse_entry_node(xmlTextReaderPtr reader){
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
            }else if (!xmlStrcmp(name, (xmlChar *)"title")){
                ret = xmlTextReaderRead(reader);
                status->text = (const char *)xmlTextReaderValue(reader);
            }else if (!xmlStrcmp(name, (xmlChar *)"author")){
                status->user = twitter_parse_author_node(reader);
            }
            xmlFree(name);
        } else if (type == XML_READER_TYPE_END_ELEMENT){
            name = xmlTextReaderName(reader);
            if (!xmlStrcmp(name, (xmlChar *)"entry")){
                xmlFree(name);
                break;
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return status;
}

GList* twitter_parse_atom(xmlTextReaderPtr reader)
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
            if(!xmlStrcmp(name, (xmlChar *)"entry")) {
                status = twitter_parse_entry_node(reader);
                if(status){
                    statuses = g_list_append(statuses, status);
                }
            }
            xmlFree(name);
        }
    }while(ret == 1);
    return statuses;
}

GList* twitter_search_timeline(twitter_t *twitter, const char *word)
{
    int ret;
    GList *timeline = NULL;
    GByteArray *buf;
    xmlTextReaderPtr reader;
    char api_uri[PATH_MAX];
    char api_param[PATH_MAX];
    twitter_status_t *status;
    char *id;

    if(twitter->lang){
        snprintf(api_param, PATH_MAX, "since_id=%lld&lang=%s&q=%s",
                 twitter->last_friends_timeline,
                 twitter->lang,
                 word);
    }else{
        snprintf(api_param, PATH_MAX, "since_id=%lld&q=%s",
                 twitter->last_friends_timeline,
                 word);
    }
    snprintf(api_uri, PATH_MAX, "%s?%s", TWITTER_SEARCH_URI, api_param);

    if(twitter->debug > 1){
        printf("search word: %s\n", word);
        printf("api_uri: %s\n", api_uri);
    }
    buf = g_byte_array_new();

    ret = twitter_fetch(twitter, api_uri, buf);
    if(ret){
        printf("ERROR: twitter_fetch()\n");
        return NULL;
    }
    if(twitter->debug >= 3){
        printf("%s\n", buf->data);
    }
    reader = xmlReaderForMemory((const char *)buf->data, buf->len,
                                NULL, NULL, 0);
    timeline = twitter_parse_atom(reader);
    xmlFreeTextReader(reader);
    g_byte_array_free (buf, TRUE);
    xmlMemoryDump();

    if(timeline){
        status = timeline->data;
        id = strchr(status->id, ':');
        if(id){
            id = strchr(++id, ':');
            id++;
            twitter->last_friends_timeline = atoll(id);
        }else{
            twitter->last_friends_timeline = 0;
        }
    }
    return timeline;
}

int twitter_count(const char *text){
    int i=0;
    unsigned char c;
    int count=0;

    while(text[i]){
        count++;
        c = (unsigned char)text[i];
        if(c < 0x80){
            i++;
            continue;
        }
        while(c & 0x80){
            c<<=1;
            i++;
        }
    }
    return count;
}

int twitter_shorten_tinyurl(twitter_t *twitter,
                            const char *url,
                            char *shorten_url){
    CURL *curl;
    CURLcode code;
    long res;
    char shortener_url[PATH_MAX];

    GByteArray *buf;
    buf = g_byte_array_new();

    curl = curl_easy_init();
    if(!curl) {
        printf("error: curl_easy_init()\n");
        return -1;
    }
    snprintf(shortener_url, PATH_MAX,
             "http://tinyurl.com/api-create.php?url=%s",
             url);
    if(twitter->debug > 0){
        printf("shortener_url: %s\n", shortener_url);
    }

    curl_easy_setopt(curl, CURLOPT_URL, shortener_url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buf);

    code = curl_easy_perform(curl);
    if(code){
        printf("error: %s\n", curl_easy_strerror(code));
        return -1;
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
    curl_easy_cleanup(curl);
    if(res != 200){
        printf("error respose code: %ld\n", res);
        return res;
    }
    strncpy(shorten_url, (const char*)buf->data, buf->len);
    if(buf->len < PATH_MAX){
        shorten_url[buf->len] = '\0';
    }else{
        shorten_url[PATH_MAX - 1] = '\0';
    }
    return 0;
}

int twitter_shorten(twitter_t *twitter, const char *text, char *shortentext){
    regex_t url_regex;
    int ret;
    regmatch_t match[1];
    int i = 0;
    int j = 0;
    char url[PATH_MAX];
    char shorten_url[PATH_MAX];
    size_t shorten_len;

    regcomp(&url_regex, "^http://[^ ]*", REG_EXTENDED);
    while(text[i]){
        ret = regexec(&url_regex, text+i, 1, match, 0);
        if(ret != REG_NOMATCH && match[0].rm_so >= 0){
            strncpy(url, text + i, match[0].rm_eo);
            url[match[0].rm_eo] = '\0';
            ret = twitter_shorten_tinyurl(twitter, url, shorten_url);
            if(ret){
                printf("shorten error: %d\n", ret);
                return -1;
            }
            printf("url: %s -> %s\n", url, shorten_url);
            shorten_len = strlen(shorten_url);
            i+=match[0].rm_eo;
            strncpy(shortentext + j, shorten_url, shorten_len);
            j+=shorten_len;
        }else{
            shortentext[j] = text[i];
            i++;
            j++;
        }
    }
    shortentext[j] = '\0';
    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
