#include "Game.h"

#include "JavaInstallations.h"
#include "Version.h"
#include "AssetIndex.h"
#include "FileSystem.h"
#include "Network.h"
#include "StringUtils.h"
#include "Logger.h"
#include "Mod.h"
#include "ProcessHandler.h"
#include "Betacraft.h"
#include "StringUtils.h"

#include <stdio.h>
#include <string.h>
#include <json-c/json.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <pwd.h>
#endif

int bc_game_run_progress = 0;

void bc_clear_natives(const char* instanceDir) {
    bc_file_list_array* arr = bc_file_list(instanceDir);
    if (arr->len == 0) {
        free(arr);
        return;
    }

    for (int i = 0; i < arr->len; i++) {
        if (arr->arr[i].is_directory) {
            if (str_starts_with(arr->arr[i].name, "natives-") && (strlen(arr->arr[i].name) == 44)) {
                char* absoluteDir = bc_file_absolute_path(instanceDir);

                char dirPath[PATH_MAX];
                snprintf(dirPath, sizeof(dirPath), "%s%s/", absoluteDir, arr->arr[i].name);
                free(absoluteDir);

                bc_file_directory_remove(dirPath);
            }
        }
    }

    free(arr);
}

void bc_game_download(const char* url, int size, const char* fileloc) {
    if (!bc_file_exists(fileloc) || bc_file_size(fileloc) != size) {
        make_path(fileloc, 1);

        bc_network_download(url, fileloc, 1);
    }
}

void bc_game_version_download(bc_version* json) {
    char fileLoc[PATH_MAX];
    snprintf(fileLoc, sizeof(fileLoc), "versions/%s.jar", json->id);

    bc_game_download(json->downloads.client.url, json->downloads.client.size, fileLoc);
}

int bc_game_rule_match(bc_version_actionRule* rule, bc_game_data* data) {
    if (!rule->features.is_empty) {
        if (rule->features.is_demo_user != -1
            && rule->features.is_demo_user == !(data->account->account_type == BC_ACCOUNT_UNAUTHENTICATED)) {
            return 0;
        }

        if (rule->features.has_server != -1
            && rule->features.has_server == !(data->server_ip[0] != '\0')) {
            return 0;
        }
    }
    if (!rule->os.is_empty) {
        if (rule->os.arch[0] != '\0') {
            // wants x86
            if (strcmp(rule->os.arch, "x86") == 0) {
#ifdef _M_X64

#else
                return 0;
#endif
            } else if (strcmp(rule->os.arch, "aarch64") == 0) {
#ifdef __aarch64__

#else
                return 0;
#endif
            } else {
                bc_log("%s\n", rule->os.arch);
            }
        }

        if (rule->os.name[0] != '\0') {
            if (strcmp(rule->os.name, "osx") == 0) {
#ifdef __APPLE__

#else
                return 0;
#endif
            } else if (strcmp(rule->os.name, "windows") == 0) {
#ifdef _WIN32

#else
                return 0;
#endif
            } else if (strcmp(rule->os.name, "linux") == 0) {
#ifdef __linux__

#else
                return 0;
#endif
            } else {
                bc_log("%s\n", rule->os.name);
            }
        }
        // ignore version for now
    }

    return 1;
}

int bc_game_rules_output(bc_version_actionRule* ruleList, int len, bc_game_data* data) {
    char action[128];
    strcpy(action, "dissalow");

    for (int i = 0; i < len; i++) {
        bc_version_actionRule* rule = &ruleList[i];

        if (bc_game_rule_match(rule, data)) {
            snprintf(action, sizeof(action), "%s", rule->action);
        }
    }

    return strcmp("allow", action) == 0;
}

char* bc_game_library_native_get_name(bc_version_nativeMap* map, int len) {
    for (int i = 0; i < len; i++) {
        bc_version_nativeMap* native = &map[i];

#ifdef _WIN32
        if (strcmp(native->os, "windows") != 0) {
            continue;
        }
#elif __linux__
        if (strcmp(native->os, "linux") != 0) {
            continue;
        }
#elif __APPLE__
        if (strcmp(native->os, "osx") != 0) {
            continue;
        }
#endif

        return fill_properties(native->classifierId, NULL);
    }

    return NULL;
}

char* bc_game_library_path(bc_version_library* lib) {
    char** pathParts = split_str(lib->name, ":");
    char alterable[1024];
    snprintf(alterable, sizeof(alterable), "%s", lib->name);
    int size = count_substring(alterable, ':');

    char* libPath;
    if (size == 3) {
        libPath = malloc(strlen(pathParts[0]) + strlen("///--") + 2 * strlen(pathParts[1]) + 2 * strlen(pathParts[2]) + strlen(pathParts[3]) + 1);
        sprintf(libPath, "%s/%s/%s/%s-%s-%s", repl_str(pathParts[0], ".", "/"), pathParts[1], pathParts[2], pathParts[1], pathParts[2], pathParts[3]);
    } else if (size == 2) {
        libPath = malloc(strlen(pathParts[0]) + strlen("///-") + 2 * strlen(pathParts[1]) + 2 * strlen(pathParts[2]) + 1);
        sprintf(libPath, "%s/%s/%s/%s-%s", repl_str(pathParts[0], ".", "/"), pathParts[1], pathParts[2], pathParts[1], pathParts[2]);
    } else {
        exit(1);
    }

    free(pathParts);
    return libPath;
}

void bc_game_download_lib(bc_version_library* lib, bc_game_data* data) {
    if (lib->rules_len > 0
        && !bc_game_rules_output(lib->rules, lib->rules_len, data)) {
        return; // disallowed
    }

    char* libPath = bc_game_library_path(lib);

    if (lib->downloads.artifact.size > 0) {
        char fileLoc[PATH_MAX];
        snprintf(fileLoc, sizeof(fileLoc), "libraries/%s.jar", libPath);

        bc_game_download(lib->downloads.artifact.url, lib->downloads.artifact.size, fileLoc);
    }

    if (lib->downloads.classifiers_len > 0 && lib->natives_len > 0) {
        char* native = bc_game_library_native_get_name(lib->natives, lib->natives_len);

        // we don't free NULL, so this if is needed
        if (native != NULL) {
            for (int i = 0; i < lib->downloads.classifiers_len; i++) {
                bc_version_classifiersMap* map = &lib->downloads.classifiers[i];

                if (strcmp(native, map->id) != 0) {
                    // native not for this os
                    continue;
                }

                char fileLoc[PATH_MAX];
                snprintf(fileLoc, sizeof(fileLoc), "libraries/%s-%s.jar", libPath, map->id);

                bc_game_download(map->object.url, map->object.size, fileLoc);
            }

            free(native);
        }
    }

    free(libPath);
}

char* bc_game_get_assets_root() {
    char assetsDir[32];
    char* location, * path;

#ifdef __APPLE__
    location = bc_file_directory_get_working();
    strcpy(assetsDir, "/minecraft/assets/");
#else
#ifdef _WIN32
    location = getenv("APPDATA");
#elif __linux__
    struct passwd* pw = getpwuid(getuid());
    location = pw->pw_dir;
#endif
    strcpy(assetsDir, "/.minecraft/assets/");
#endif    

    path = malloc(strlen(location) + strlen(assetsDir) + 1);
    strcpy(path, location);
    strcat(path, assetsDir);

#ifdef __APPLE__
    free(location);
#endif

    return path;
}

void bc_game_download_assets(bc_assetindex* ai) {
    for (int i = 0; i < ai->len; i++) {
        bc_assetindex_asset* obj = &ai->objects[i];

        char* assetsDir = bc_game_get_assets_root();
        char assetsLoc[PATH_MAX];
        snprintf(assetsLoc, sizeof(assetsLoc), "%sobjects/%c%c/%s", assetsDir, obj->hash[0], obj->hash[1], obj->hash);

        if (obj->baseUrl[0] == '\0') {
            int sizeObj = strlen("https://resources.download.minecraft.net//") + 2 + strlen(obj->hash) + 1;
            snprintf(obj->baseUrl, sizeObj, "https://resources.download.minecraft.net/%c%c/%s", obj->hash[0], obj->hash[1], obj->hash);
        }

        bc_game_download(obj->baseUrl, obj->size, assetsLoc);
        free(assetsDir);
    }
}

char* bc_game_classpath(bc_game_data* data) {
    char colon[2];
#ifdef _WIN32
    strcpy(colon, ";");
#else
    strcpy(colon, ":");
#endif
    char* workDir = bc_file_directory_get_working();
    char versionPath[PATH_MAX];

    if (data->mods->len > 0) {
        snprintf(versionPath, sizeof(versionPath), "%sbc_instance.jar", data->instance->path);
        bc_mod_install(data->mods, data->instance->path, data->instance->version);
    } else {
        snprintf(versionPath, sizeof(versionPath), "%sversions/%s.jar", workDir, data->version->id);
    }

    char* classPath = malloc(strlen(versionPath) + 1);
    strcpy(classPath, versionPath);

    for (int i = 0; i < data->version->lib_len; i++) {
        bc_version_library* lib = &data->version->libraries[i];
        if (lib->rules_len > 0
            && !bc_game_rules_output(lib->rules, lib->rules_len, data)) {
            continue;
        }

        char filePath[PATH_MAX];
        char* libPath = bc_game_library_path(lib);

        snprintf(filePath, sizeof(filePath), "%slibraries/%s.jar", workDir, libPath);

        classPath = realloc(classPath, strlen(classPath) + strlen(filePath) + 1 + 1);
        sprintf(classPath, "%s%s%s", classPath, colon, filePath);

        free(libPath);
    }

    return classPath;
}

char* fill_properties(const char* input, bc_game_data* data) {
    char* replaced = strdup(input);
    if ((size_t)-1 > 0xffffffffUL) {
        replaced = repl_str(input, "${arch}", "64");
    } else {
        replaced = repl_str(input, "${arch}", "32");
    }

    if (strstr(replaced, "${launcher_name}") != NULL) {
        replaced = repl_str(replaced, "${launcher_name}", "Betacraft");
    }

    if (strstr(replaced, "${launcher_version}") != NULL) {
        replaced = repl_str(replaced, "${launcher_version}", BETACRAFT_VERSION);
    }

    if (strstr(replaced, "${assets_root}") != NULL) {
        char* path = bc_game_get_assets_root();

        replaced = repl_str(replaced, "${assets_root}", path);
        bc_log("\nassets_root=%s\n", replaced);
        free(path);
    }

    if (strstr(replaced, "${game_assets}") != NULL) {
        char virtualassets[PATH_MAX];
        char* path = bc_game_get_assets_root();

        snprintf(virtualassets, sizeof(virtualassets), "%svirtual/%s/", path, data->version->assets);
        replaced = repl_str(replaced, "${game_assets}", bc_file_make_absolute_path(virtualassets));
        bc_log("\ngame_assets=%s\n", replaced);
        free(path);
    }

    if (data != NULL) {
        // complex stuff, execute as rarely as possible
        if (strstr(replaced, "${classpath}") != NULL) {
            char* cp = bc_game_classpath(data);
            replaced = repl_str(replaced, "${classpath}", cp);
            free(cp);
        }

        if (strstr(replaced, "${resolution_width}") != NULL) {
            char widthstr[16];
            snprintf(widthstr, sizeof(widthstr), "%i", data->instance->width);
            replaced = repl_str(replaced, "${resolution_width}", widthstr);
        }

        if (strstr(replaced, "${resolution_height}") != NULL) {
            char heightstr[16];
            snprintf(heightstr, sizeof(heightstr), "%i", data->instance->height);
            replaced = repl_str(replaced, "${resolution_height}", heightstr);
        }

        replaced = repl_str(replaced, "${username}", data->account->username);
        replaced = repl_str(replaced, "${auth_player_name}", data->account->username);

        if (strstr(replaced, "${auth_uuid}") != NULL) {
            char uuid_nodash[48];
            snprintf(uuid_nodash, sizeof(uuid_nodash), "%s", data->account->uuid);
            replaced = repl_str(replaced, "${auth_uuid}", uuid_nodash);
        }

        replaced = repl_str(replaced, "${auth_access_token}", data->account->access_token);
        replaced = repl_str(replaced, "${access_token}", data->account->access_token);

        if (strstr(replaced, "${auth_session}") != NULL) {
            char session[2048];
            snprintf(session, sizeof(session), "token:%s:%s", data->account->access_token, data->account->uuid);
            replaced = repl_str(replaced, "${auth_session}", session);
        }

        replaced = repl_str(replaced, "${user_properties}", "{}");

        if (data->account->account_type == BC_ACCOUNT_MOJANG) {
            replaced = repl_str(replaced, "${user_type}", "mojang");
        } else if (data->account->account_type == BC_ACCOUNT_MICROSOFT) {
            replaced = repl_str(replaced, "${user_type}", "msa");
        } else {
            replaced = repl_str(replaced, "${user_type}", "legacy");
        }

        // priority temporary over permanent
        if (data->server_ip[0] != '\0') {
            replaced = repl_str(replaced, "${server_ip}", data->server_ip);

            if (data->server_port[0] != '\0') {
                replaced = repl_str(replaced, "${server_port}", data->server_port);
            } else {
                replaced = repl_str(replaced, "${server_port}", "25565");
            }
        } else if (data->instance->server_ip[0] != '\0') {
            replaced = repl_str(replaced, "${server_ip}", data->instance->server_ip);

            if (data->instance->server_port[0] != '\0') {
                replaced = repl_str(replaced, "${server_port}", data->instance->server_port);
            } else {
                replaced = repl_str(replaced, "${server_port}", "25565");
            }
        }

        replaced = repl_str(replaced, "${loadmap_id}", data->loadmap_id);
        replaced = repl_str(replaced, "${loadmap_user}", data->loadmap_user);

        if (strstr(replaced, "${game_directory}") != NULL) {
            char mcpath[PATH_MAX];
            snprintf(mcpath, sizeof(mcpath), "%s%s", data->instance->path, ".minecraft/");

            replaced = repl_str(replaced, "${game_directory}", mcpath);
        }

        replaced = repl_str(replaced, "${version_name}", data->version->id);
        replaced = repl_str(replaced, "${assets_index_name}", data->version->assetIndex.id);
        replaced = repl_str(replaced, "${version_type}", data->version->type);
        replaced = repl_str(replaced, "${profile_name}", data->instance->name);

        if (strstr(replaced, "${instance_icon}") != NULL) {
            char iconpath[PATH_MAX];
            snprintf(iconpath, sizeof(iconpath), "%s%s", data->instance->path, "instance_icon.png");

            replaced = repl_str(replaced, "${instance_icon}", iconpath);
        }

        replaced = repl_str(replaced, "${clientid}", "-");
        replaced = repl_str(replaced, "${auth_xuid}", "-");

        replaced = repl_str(replaced, "${natives_directory}", data->natives_folder);
    }

    return replaced;
}

void bc_game_run_cmd(bc_process_args* gameArgs, bc_game_data* data) {
    char* predir = getcwd(NULL, PATH_MAX);
    chdir(data->instance->path);

#ifdef _WIN32
    char* preenv = getenv("APPDATA");

    char envar[PATH_MAX];
    snprintf(envar, sizeof(envar), "APPDATA=%s", data->instance->path);

    putenv(envar);
#endif

    if (data->instance->show_log == 0) {
        bc_process_create(gameArgs);
    } else {
        bc_process_create_log(gameArgs);
    }

    // Reset to default
    chdir(predir);
#ifdef _WIN32
    putenv(preenv);
#endif
}

void bc_game_download_lib_all(bc_game_data* data) {
    for (int i = 0; i < data->version->lib_len; i++) {
        bc_version_library* lib = &data->version->libraries[i];
        bc_game_download_lib(lib, data);

        char* name = bc_game_library_native_get_name(lib->natives, lib->natives_len);

        if (name != NULL) {
            for (int i = 0; i < lib->downloads.classifiers_len; i++) {
                bc_version_classifiersMap* map = &lib->downloads.classifiers[i];

                if (strcmp(name, map->id) != 0) // Native not for this os
                    continue;

                char* libPath = bc_game_library_path(lib);

                char fileLoc[PATH_MAX];
                snprintf(fileLoc, sizeof(fileLoc), "libraries/%s-%s.jar", libPath, map->id);

                bc_log("%s\n", fileLoc);

                make_path(data->natives_folder, 0);

                bc_file_unzip(fileLoc, data->natives_folder);

                free(libPath);
            }

            free(name);
        }
    }
}

void bc_game_concat_properties(bc_game_data* data, bc_version_argRule* rules, int rules_len, bc_process_args* gameArgs) {
    for (int i = 0; i < rules_len; i++) {
        bc_version_argRule* rule = &rules[i];

        if (rule->rules_len > 0
            && !bc_game_rules_output(rule->rules, rule->rules_len, data)) {
            continue;
        }

        if (rule->value_len == 0) {
            gameArgs->arr[gameArgs->len] = fill_properties(rule->value[0], data);
            gameArgs->len++;
        } else {
            for (int j = 0; j < rule->value_len; j++) {
                gameArgs->arr[gameArgs->len] = fill_properties(rule->value[j], data);
                gameArgs->len++;
            }
        }
    }
}

void bc_game_set_args(char* args, bc_process_args* gameArgs) {
    char* p = NULL;
    p = strtok(args, "\n");

    while (p != NULL) {
        gameArgs->arr[gameArgs->len] = malloc(strlen(p) + 1);
        strcpy(gameArgs->arr[gameArgs->len], p);
        gameArgs->len++;

        p = strtok(NULL, "\n");
    }
}

void bc_game_run(bc_game_data* data) {
    bc_game_run_progress = 25;
    bc_clear_natives(data->instance->path);

    bc_game_version_download(data->version);
    bc_game_run_progress = 50;
    bc_game_download_lib_all(data);

    bc_assetindex* ai = bc_assetindex_load(&data->version->assetIndex);
    bc_game_run_progress = 75;
    bc_game_download_assets(ai);

    free(ai->objects);
    free(ai);

    bc_jinst* javaInstall = bc_jinst_get(data->instance->java_path);

    if (javaInstall == NULL) {
        bc_log("%s\n", "Error: bc_game_run - Java installation not found");
        return;
    }

    bc_process_args gameArgs;

    char* userHome = malloc(strlen("-Duser.home=") + strlen(data->instance->path) + 1);
    sprintf(userHome, "-Duser.home=%s", data->instance->path);

    gameArgs.size = 0;
    gameArgs.arr[0] = javaInstall->path;
    gameArgs.arr[1] = userHome;
    gameArgs.len = 2;

    bc_game_set_args(data->instance->jvm_args, &gameArgs);

    bc_game_concat_properties(data, data->version->arguments.jvm, data->version->arguments.jvm_len, &gameArgs);

    gameArgs.arr[gameArgs.len] = malloc(strlen(data->version->mainClass) + 1);
    strcpy(gameArgs.arr[gameArgs.len], data->version->mainClass);
    gameArgs.len++;

    bc_game_set_args(data->instance->program_args, &gameArgs);

    bc_game_concat_properties(data, data->version->arguments.game, data->version->arguments.game_len, &gameArgs);

    bc_game_run_progress = 100;

    for (int i = 0; i < gameArgs.len; i++) {
        gameArgs.size += strlen(gameArgs.arr[i]) + 1;
        bc_log("%s\n", gameArgs.arr[i]);
    }

    bc_game_run_cmd(&gameArgs, data);

    free(javaInstall);

    for (int i = 1; i < gameArgs.len; i++) {
        free(gameArgs.arr[i]);
    }
}
