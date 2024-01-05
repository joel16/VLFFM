#pragma once

typedef struct {
	int background_number;
} Config;

extern Config config;

int configSave(void);
int configLoad(void);
