#ifndef MAIN_H
#define MAIN_H

#include "transformer_design.h"

int loadConfiguration(Transformer *tx, const char *filename);
int applyConfigurationValue(Transformer *tx, const char *key, const char *value);
void setDefaultConfiguration(Transformer *tx);

#endif
