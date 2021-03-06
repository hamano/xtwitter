/*
 * libtwitter.c - Xtwitter: twitter client for X
 * Copyright (C) 2008-2015 HAMANO Tsukasa <code@cuspy.org>
 * This software is released under the MIT License.
 * See LICENSE file for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <curl/curl.h>
#include <Imlib2.h>
#include <oauth.h>
#include <regex.h>
#include <json/json.h>
#include <termio.h>
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

static int secure_input(char *passwd, size_t size, int mask){
    int c;
    size_t len = 0;
    struct termio term, term_orig;

    ioctl(0, TCGETA, &term_orig);
    term = term_orig;
    //term.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
    term.c_lflag &= ~(ICANON|ECHO);
    ioctl(0, TCSETA, &term);

    while((c = getchar()) != '\n'){
        //printf("hex=%x\n", c);
        if(c == '\b' || c == 0x7f){
            if(len > 0){
                putchar('\b');
                putchar(' ');
                putchar('\b');
                len--;
            }
            continue;
        }
        if(len >= size - 1){
            break;
        }
        if(mask == 0){
            putchar(c);
        }else{
            putchar(mask);
        }
        passwd[len++] = c;
    }
    passwd[len] = '\0';
    putchar('\n');
    ioctl(0, TCSETA, &term_orig);

    if(len == 0){
        return 1;
    }else if(len >= size){
        return 2;
    }else{
        return 0;
    }
}


int twitter_setup_account(const char *config_file){
    int ret;
    char username[256];
    char password[256];
    FILE *fp;

    printf("Username: ");
    while((ret = secure_input(username, sizeof(username), 0)) != 0){
        if(ret == 1){
            printf("Too short\n");
        }else{
            printf("Too long\n");
        }
        printf("Retry Input Username: ");
    }

    printf("Password: ");
    while((ret = secure_input(password, sizeof(password), '*')) != 0){
        if(ret == 1){
            printf("Too short\n");
        }else{
            printf("Too long\n");
        }
        printf("Retry Input Password: ");
    }
    fp = fopen(config_file, "w");
    if(!fp){
        return -1;
    }
    fprintf(fp, "user=%s\n", username);
    fprintf(fp, "pass=%s\n", password);
    fclose(fp);
    return 0;
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

    if(stat(config_dir, &st) && errno == ENOENT){
        mkdir(config_dir, 0755);
    }

    if(stat(twitter->images_dir, &st) && errno == ENOENT){
        mkdir(twitter->images_dir, 0755);
    }

    if(stat(config_file, &st) && errno == ENOENT){
        twitter_setup_account(config_file);
    }

    fp = fopen(config_file, "r");
    if(!fp){
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
    buf_t *buf = data;
    size_t realsize = size * nmemb;
    if(realsize < buf->cap){
        memcpy(buf->data, ptr, realsize);
        buf->len = realsize;
    }else{
        memcpy(buf->data, ptr, buf->cap - 1);
        buf->data[buf->cap - 1] = '\0';
        buf->len = buf->cap - 1;
        return buf->cap - 1;
    }
    return realsize;
}

#if 0
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
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
#endif

static void twitter_print_json(char *str){
    int indent = 0;
    int c;
    int i;
    while((c = *str++) != '\0'){
        putchar(c);
        if(c == '{'){
            indent++;
            putchar('\n');
            for(i=0; i<indent; i++){putchar(' '); putchar(' ');}
        }
        if(c == '}'){
            indent--;
        }
        if(c == ','){
            putchar('\n');
            for(i=0; i<indent; i++){putchar(' '); putchar(' ');}
        }
    }
    putchar('\n');
}

static char *json_obj_strdup(json_object *obj, char *key){
    const char *str = json_object_get_string(json_object_object_get(obj, key));
    if(str){
        return strdup(str);
    }
    return NULL;
}

static twitter_status_t *twitter_parse_status(twitter_t *twitter,
                                              json_object *obj_root,
                                              json_object *obj_user)
{
    twitter_status_t *status = twitter_status_new();
    twitter_status_t *status_rt = NULL;
    json_object *obj_rt = NULL;
    json_object *obj_rt_user = NULL;

    status->created_at = json_obj_strdup(obj_root, "created_at");
    status->id = json_obj_strdup(obj_root, "id_str");
    status->source = json_obj_strdup(obj_root, "source");
    status->text = json_obj_strdup(obj_root, "text");
    status->user_id = json_obj_strdup(obj_user, "id");
    status->user_name = json_obj_strdup(obj_user, "name");
    status->user_screen_name = json_obj_strdup(obj_user, "screen_name");
    status->user_profile_image_url =
        json_obj_strdup(obj_user, "profile_image_url");

    obj_rt = json_object_object_get(obj_root, "retweeted_status");
    if(obj_rt){
        status_rt = twitter_status_new();
        status_rt->created_at = json_obj_strdup(obj_rt, "created_at");
        status_rt->id = json_obj_strdup(obj_rt, "id_str");
        status_rt->source = json_obj_strdup(obj_rt, "source");
        status_rt->text = json_obj_strdup(obj_rt, "text");
        obj_rt_user = json_object_object_get(obj_rt, "user");
        status_rt->user_id = json_obj_strdup(obj_rt_user, "id");
        status_rt->user_name = json_obj_strdup(obj_rt_user, "name");
        status_rt->user_screen_name = json_obj_strdup(obj_rt_user, "screen_name");
        status_rt->user_profile_image_url =
            json_obj_strdup(obj_rt_user, "profile_image_url");
        status->rt = (struct twitter_status_t *)status_rt;
    }else{
        status->rt = NULL;
    }

    return status;
}

static size_t twitter_curl_stream_cb(void *ptr, size_t size, size_t nmemb,
                                     void *data)
{
    //json_tokener *tokener;
    twitter_t *twitter = data;
    size_t realsize = size * nmemb;
    json_object *obj_root = NULL;
    json_object *obj_tmp = NULL;

    if(realsize <= 2){
        return realsize;
    }
    //tokener = json_tokener_new();
    //obj_root = json_tokener_parse_ex(tokener, ptr, realsize);
    //json_tokener_free(tokener);
    obj_root = json_tokener_parse(ptr);
    /*
    if(!obj_root){
        fprintf(stderr, "obj_root is NULL\n");
        return realsize;
    }
    */
    if(is_error(obj_root)){
        fprintf(stderr, "json_tokener_errors: %s\n", json_tokener_errors[-(unsigned long)obj_root]);
        fprintf(stderr, "parse error: realsize=%ld\n", realsize);
        fprintf(stderr, "ptr=%s\n", (char *)ptr);
        return realsize;
    }
    obj_tmp = json_object_object_get(obj_root, "friends");
    if(obj_tmp){
        printf("watching %d friends.\n", json_object_array_length(obj_tmp));
        json_object_put(obj_root);
        return realsize;
    }

    obj_tmp = json_object_object_get(obj_root, "event");
    if(obj_tmp){
        printf("EVENT: %s\n", json_object_to_json_string(obj_tmp));
        json_object_put(obj_root);
        return realsize;
    }

    obj_tmp = json_object_object_get(obj_root, "delete");
    if(obj_tmp){
        printf("DELETE: %s\n", json_object_to_json_string(obj_root));
        json_object_put(obj_root);
        return realsize;
    }

    obj_tmp = json_object_object_get(obj_root, "disconnect");
    if(obj_tmp){
        printf("DISCONNECT: %s\n", json_object_to_json_string(obj_root));
        json_object_put(obj_root);
        return realsize;
    }

    obj_tmp = json_object_object_get(obj_root, "user");
    if(obj_tmp){
        twitter_status_t *status = NULL;
        //twitter_print_json(ptr);
        status = twitter_parse_status(twitter, obj_root, obj_tmp);
        json_object_put(obj_root);
        json_object_put(obj_tmp);
        twitter_stat_image(twitter, status);
        if(twitter->debug > 1){
            twitter_status_dump(status);
        }else if(twitter->quiet == 0){
            twitter_status_print(status);
        }
        if(twitter->popup){
            twitter->popup(twitter, status);
        }
        twitter_status_free(status);
        return realsize;
    }

    fprintf(stderr, "unknown object\n");
    fprintf(stderr, "%s\n", json_object_to_json_string(obj_root));
    json_object_put(obj_root);
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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_stream_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)twitter);
    //curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    // 2010-07-20 bug
    //curl_easy_setopt(curl, CURLOPT_IGNORE_CONTENT_LENGTH, 1);

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
    buf_t *buf = NULL;
    char *req_uri;

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
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_curl_write_cb);
    buf = buf_new(4096);
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
    }else{
        res = 0;
    }
    buf_free(buf);
    curl_easy_cleanup(curl);
    curl_formfree(formpost);
    curl_slist_free_all(headers);
    return res;
}

void twitter_user_stream(twitter_t *twitter)
{
    char api_uri[PATH_MAX];
    snprintf(api_uri, PATH_MAX, "%s", TWITTER_USER_STREAM_URI);
    char *req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "GET",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug >= 2){
        printf("req_uri: %s\n", req_uri);
    }

    twitter_user_stream_read(twitter, req_uri);
}

void twitter_public_stream(twitter_t *twitter)
{
    char api_uri[PATH_MAX];
    snprintf(api_uri, PATH_MAX, "%s", TWITTER_PUBLIC_STREAM_URI);
    char *req_uri = oauth_sign_url2(
        api_uri, NULL, OA_HMAC, "GET",
        twitter->consumer_key, twitter->consumer_secret,
        twitter->token_key, twitter->token_secret);

    if(twitter->debug >= 2){
        printf("req_uri: %s\n", req_uri);
    }

    twitter_user_stream_read(twitter, req_uri);
}

/*
 * twitter unescape only &lt; and &gt;
 */
void twitter_unescape(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n);
    char *p;
    while((p = strstr(dst, "&lt;")) != NULL){
        *p++ = '<';
        while((*p = *(p + 3))) p++;
        *p = '\0';
    }
    while((p = strstr(dst, "&gt;")) != NULL){
        *p++ = '>';
        while((*p = *(p + 3))) p++;
        *p = '\0';
    }
}

/*
  XML escape only &, > and <;
 */
void twitter_xmlescape(char *dest, const char *src, size_t n)
{
    int i = 0;
    int j = 0;
    do{
        if(j + 6 > n){
            dest[j] = '\0';
            break;
        }else if(src[i] == '&'){
            dest[j++] = '&';
            dest[j++] = 'a';
            dest[j++] = 'm';
            dest[j++] = 'p';
            dest[j++] = ';';
        }else if(src[i] == '>'){
            dest[j++] = '&';
            dest[j++] = 'g';
            dest[j++] = 't';
            dest[j++] = ';';
        }else if(src[i] == '<'){
            dest[j++] = '&';
            dest[j++] = 'l';
            dest[j++] = 't';
            dest[j++] = ';';
        }else{
            dest[j] = src[i];
            j++;
        }
    }while(src[i++]);
}

void twitter_status_print(twitter_status_t *status){
    char text[2048];
    twitter_status_t *rt = (twitter_status_t *)status->rt;

    if(rt){
        twitter_unescape(text, rt->text, 2048);
        printf("@%s: RT @%s: %s\n",
               status->user_screen_name, rt->user_screen_name, rt->text);
    }else{
        twitter_unescape(text, status->text, 2048);
        printf("@%s: %s\n", status->user_screen_name, text);
    }
}

void twitter_status_dump(twitter_status_t *status){
    printf("[%s] @%s: %s\n", status->id,
           status->user_screen_name, status->text);
}

twitter_status_t *twitter_status_new(){
    twitter_status_t *status;
    status = (twitter_status_t *)malloc(sizeof(twitter_status_t));
    memset(status, 0, sizeof(twitter_status_t));
    return status;
}

void twitter_status_free(twitter_status_t *status){
    if(status->created_at) free((void *)status->created_at);
    if(status->id) free((void *)(status->id));
    if(status->text) free((void *)(status->text));
    if(status->source) free((void *)(status->source));
    if(status->user_id) free((void *)(status->user_id));
    if(status->user_name) free((void *)(status->user_name));
    if(status->user_screen_name) free((void *)(status->user_screen_name));
    if(status->user_profile_image_url)
        free((void *)(status->user_profile_image_url));
    if(status->rt){
        twitter_status_free((twitter_status_t *)status->rt);
    }
    free(status);
}

int twitter_image_name(twitter_status_t *status, char *name){
    size_t i;
    i = strlen(status->user_profile_image_url);
    while(i--)
        if(status->user_profile_image_url[i] == '/')
            break;
    while(i--)
        if(status->user_profile_image_url[i] == '/')
            break;
    i++;
    strncpy(name, status->user_profile_image_url + i, PATH_MAX - 1);
    i = strlen(name);
    while(i--)
        if(name[i] == '/')
            name[i] = '_';
    return 0;
}

int twitter_stat_image(twitter_t *twitter, twitter_status_t *status){
    int ret;
    char name[PATH_MAX];
    char path[PATH_MAX];
    const char *url;
    struct stat st;

    twitter_image_name(status, name);
    url = status->user_profile_image_url;
    snprintf(path, PATH_MAX, "%s/%s", twitter->images_dir, name);
    ret = stat(path, &st);
    if(ret){
        twitter_fetch_image(twitter, url, path);
        twitter_resize_image(twitter, path);
    }
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

    buf_t *buf;

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
    buf = buf_new(1024);
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
            //ret = twitter_shorten_tinyurl(twitter, url, shorten_url);
            ret=-1;
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

buf_t *buf_new(size_t cap){
    buf_t *buf;
    buf = (buf_t *)malloc(sizeof(buf_t));
    buf->data = (char *)malloc(cap);
    buf->cap = cap;
    buf->len = 0;
    return buf;
}

void buf_free(buf_t *buf){
    free(buf->data);
    free(buf);
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
