#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fs.h"
#include "jsmn.h"
#include "utils.h"

#define CONFIG_VERSION 1

Config config;
static const char *config_file = "{\n\t\"config_version\": %d,\n\t\"background_number\": %d\n}\n";
static int config_version_holder = 0;

static bool jsonEquals(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start && strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return true;
    }
    
    return false;
}

int configSave(void) {
    int ret = 0;
    char *buf = (char *)malloc(64);
    int len = snprintf(buf, 64, config_file, CONFIG_VERSION, config.background_number);
    
    if (R_FAILED(ret = fsWriteFile("config.json", buf, len))) {
        utilsLogError("fsWriteFile(config.json) failed: 0x%lx\n", ret);
    }
    
    free(buf);
    return ret;
}

int configLoad(void) {
    int ret = 0;
    
    if (!fsFileExists("config.json")) {
        config.background_number = 16;
        return configSave();
    }
    
    SceSize size = fsGetFileSize("config.json");
    char *buf =  (char *)malloc(size + 1);
    
    if (R_FAILED(ret = fsReadFile("config.json", buf, size))) {
        utilsLogError("fsReadFile(config.json) failed: 0x%lx\n", ret);
        free(buf);
        return ret;
    }
    
    buf[size] = '\0';
    jsmn_parser parser;
    jsmntok_t token[64];
    
    jsmn_init(&parser);
    if (R_FAILED(ret = jsmn_parse(&parser, buf, strlen(buf), token, 64))) {
        utilsLogError("jsmn_parse failed: %d\n", ret);
        free(buf);
        return ret;
    }
    
    if (ret < 1 || token[0].type != JSMN_OBJECT) {
        utilsLogError("jsmn_parse failed: object expected\n");
        free(buf);
        return ret;
    }
    
    for (int i = 1; i < ret; i++) {
        if (jsonEquals(buf, &token[i], "config_version")) {
            config_version_holder = strtol(buf + token[i + 1].start, NULL, 10);
            i++;
        }
        else if (jsonEquals(buf, &token[i], "background_number")) {
            config.background_number = strtol(buf + token[i + 1].start, NULL, 10);
            i++;
        }
        else {
            utilsLogError("Unexpected key: %.*s\n", token[i].end - token[i].start, buf + token[i].start);
        }
    }
    
    free(buf);
    
    // Delete config file if config version is updated.
    if (config_version_holder < CONFIG_VERSION) {
        sceIoRemove("config.json");
        config.background_number = 16;
        return configSave();
    }
    
    return 0;
}
