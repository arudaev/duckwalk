#pragma once

/* On the host, an app's data folder is a directory the test chooses. */
extern char shim_app_data_dir[256];

#define APP_DATA_PATH(name) shim_app_data_path(name)
const char* shim_app_data_path(const char* name);

typedef struct Storage Storage;
