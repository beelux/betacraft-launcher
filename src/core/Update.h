#ifndef BC_UPDATE_H
#define BC_UPDATE_H

typedef struct bc_github_asset {
    char name[32];
    char browser_download_url[256];
} bc_github_asset;

typedef struct bc_github_release {
    char tag_name[32];
    bc_github_asset assets[8];
    int assets_length;
} bc_github_release;

void bc_update_perform();

#endif
