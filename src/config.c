#include "../include/transformer_design.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const FieldEntry fieldTable[] =
{
    /*==========================================================
      DESIGN INPUTS - General Specifications
    ==========================================================*/
    { "APPARENT_POWER",            offsetof(Transformer, input.KVA),            FIELD_DOUBLE, "kVA"    },
    { "HV_VOLTAGE",                offsetof(Transformer, input.HV),             FIELD_DOUBLE, "V"      },
    { "LV_VOLTAGE",                offsetof(Transformer, input.LV),             FIELD_DOUBLE, "V"      },
    { "PHASES",                    offsetof(Transformer, input.Ph),             FIELD_DOUBLE, "-"      },
    { "FREQUENCY",                 offsetof(Transformer, input.f),              FIELD_DOUBLE, "Hz"     },
    { "TARGET_TEMP_RISE",          offsetof(Transformer, input.TRP),            FIELD_DOUBLE, "C"      },
    { "COPPER_DENSITY",            offsetof(Transformer, input.density_cu),     FIELD_DOUBLE, "kg/m^3" },
    { "STEEL_DENSITY",             offsetof(Transformer, input.density_fe),     FIELD_DOUBLE, "kg/m^3" },
    { "OIL_DENSITY",               offsetof(Transformer, input.density_oil),    FIELD_DOUBLE, "kg/m^3" },

    /*==========================================================
      MAGNETIC FRAME & CORE GEOMETRY (LENANA) - Part 1
    ==========================================================*/
    { "CORE_STEP_FACTOR",          offsetof(Transformer, input.k),              FIELD_DOUBLE, "-"      },
    { "EMF_VALUE_FACTOR",          offsetof(Transformer, input.K),              FIELD_DOUBLE, "-"      },
    { "WINDOW_SPACE_FACTOR",       offsetof(Transformer, input.kw),             FIELD_DOUBLE, "-"      },
    { "IRON_STACKING_FACTOR",      offsetof(Transformer, input.ki),             FIELD_DOUBLE, "-"      },
    { "VOLTS_PER_TURN",            offsetof(Transformer, magneticFrame.Et),     FIELD_DOUBLE, "V/turn" },
    { "CORE_FLUX_DENSITY",         offsetof(Transformer, input.Bm),             FIELD_DOUBLE, "T"      },
    { "YOKE_FLUX_DENSITY",         offsetof(Transformer, magneticFrame.By),     FIELD_DOUBLE, "T"      },
    { "AVERAGE_CURRENT_DENSITY",   offsetof(Transformer, input.cdav),           FIELD_DOUBLE, "A/mm^2" },
    { "CORE_NET_AREA",             offsetof(Transformer, magneticFrame.Ai),     FIELD_DOUBLE, "m^2"    },
    { "CORE_GROSS_AREA",           offsetof(Transformer, magneticFrame.Ac),     FIELD_DOUBLE, "m^2"    },
    { "CORE_LIMB_DIAMETER",        offsetof(Transformer, magneticFrame.d),      FIELD_DOUBLE, "m"      },
    { "CORE_LIMB_LENGTH",          offsetof(Transformer, magneticFrame.L),      FIELD_DOUBLE, "m"      },
    { "CORE_CENTER_DISTANCE",      offsetof(Transformer, magneticFrame.D),      FIELD_DOUBLE, "m"      },
    { "WINDOW_AREA",               offsetof(Transformer, magneticFrame.Aw),     FIELD_DOUBLE, "m^2"    },
    { "YOKE_LENGTH",               offsetof(Transformer, magneticFrame.W),      FIELD_DOUBLE, "m"      },
    { "YOKE_GROSS_AREA",           offsetof(Transformer, magneticFrame.Ay),     FIELD_DOUBLE, "m^2"    },
    { "YOKE_WIDTH",                offsetof(Transformer, magneticFrame.by),     FIELD_DOUBLE, "m"      },
    { "YOKE_HEIGHT",               offsetof(Transformer, magneticFrame.hy),     FIELD_DOUBLE, "m"      },
    { "CORE_LOSS_PER_KG",          offsetof(Transformer, magneticFrame.WpKgC),  FIELD_DOUBLE, "W/kg"   },
    { "YOKE_LOSS_PER_KG",          offsetof(Transformer, magneticFrame.WpKgY),  FIELD_DOUBLE, "W/kg"   },
    { "CORE_WEIGHT",               offsetof(Transformer, magneticFrame.KgC),    FIELD_DOUBLE, "kg"     },
    { "YOKE_WEIGHT",               offsetof(Transformer, magneticFrame.KgY),    FIELD_DOUBLE, "kg"     },
    { "CORE_IRON_LOSS",            offsetof(Transformer, magneticFrame.PiC),    FIELD_DOUBLE, "W"      },
    { "YOKE_IRON_LOSS",            offsetof(Transformer, magneticFrame.PiY),    FIELD_DOUBLE, "W"      },
    { "TOTAL_IRON_LOSS_KW",        offsetof(Transformer, magneticFrame.Pi),     FIELD_DOUBLE, "kW"     },

    /*==========================================================
      NO-LOAD CURRENT EXCITATION - Part 2
    ==========================================================*/
    { "CORE_AT_PER_METER",         offsetof(Transformer, noLoadCurrent.atC),    FIELD_DOUBLE, "AT/m"   },
    { "YOKE_AT_PER_METER",         offsetof(Transformer, noLoadCurrent.atY),    FIELD_DOUBLE, "AT/m"   },
    { "CORE_TOTAL_AT",             offsetof(Transformer, noLoadCurrent.ATC),    FIELD_DOUBLE, "AT"     },
    { "YOKE_TOTAL_AT",             offsetof(Transformer, noLoadCurrent.ATY),    FIELD_DOUBLE, "AT"     },
    { "TOTAL_AT_PER_PHASE",        offsetof(Transformer, noLoadCurrent.ATpPh),  FIELD_DOUBLE, "AT"     },
    { "LV_TURNS_REF",              offsetof(Transformer, noLoadCurrent.T2),     FIELD_DOUBLE, "turns"  },
    { "LV_PHASE_CURRENT",          offsetof(Transformer, noLoadCurrent.I2),     FIELD_DOUBLE, "A"      },
    { "WATTFUL_CURRENT",           offsetof(Transformer, noLoadCurrent.Iw),     FIELD_DOUBLE, "A"      },
    { "MAGNETIZING_CURRENT",       offsetof(Transformer, noLoadCurrent.Im),     FIELD_DOUBLE, "A"      },
    { "NO_LOAD_CURRENT",           offsetof(Transformer, noLoadCurrent.I0),     FIELD_DOUBLE, "A"      },
    { "NO_LOAD_CURRENT_RATIO",     offsetof(Transformer, noLoadCurrent.I0byI2), FIELD_DOUBLE, "%"      },

    /*==========================================================
      LV WINDING (AITSA) - Part 3
    ==========================================================*/
    { "LV_AVAILABLE_HEIGHT",       offsetof(Transformer, lv.ALW),               FIELD_DOUBLE, "mm"     },
    { "LV_TURNS",                  offsetof(Transformer, lv.T),                 FIELD_DOUBLE, "turns"  },
    { "LV_TURNS_RADIALLY",         offsetof(Transformer, lv.Tr),                FIELD_DOUBLE, "turns"  },
    { "LV_TURNS_AXIALLY",          offsetof(Transformer, lv.Ta),                FIELD_DOUBLE, "turns"  },
    { "LV_PARALLEL_STRANDS",       offsetof(Transformer, lv.stP),               FIELD_DOUBLE, "strands"},
    { "LV_AXIAL_STRANDS",          offsetof(Transformer, lv.NstA),              FIELD_DOUBLE, "strands"},
    { "LV_RADIAL_STRANDS",         offsetof(Transformer, lv.NstR),              FIELD_DOUBLE, "strands"},
    { "LV_SPACE_PER_TURN",         offsetof(Transformer, lv.ALT),               FIELD_DOUBLE, "mm"     },
    { "LV_STRAND_WIDTH_BARE",      offsetof(Transformer, lv.stW),               FIELD_DOUBLE, "mm"     },
    { "LV_STRAND_THICKNESS_BARE",  offsetof(Transformer, lv.stT),               FIELD_DOUBLE, "mm"     },
    { "LV_SPACE_OCCUPIED_AXIALLY", offsetof(Transformer, lv.ALWx),              FIELD_DOUBLE, "mm"     },
    { "LV_AXIAL_SLACK",            offsetof(Transformer, lv.SlkAx),             FIELD_DOUBLE, "mm"     },
    { "LV_CONDUCTOR_AREA",         offsetof(Transformer, lv.a),                 FIELD_DOUBLE, "mm^2"   },
    { "LV_CURRENT_DENSITY",        offsetof(Transformer, lv.cd),                FIELD_DOUBLE, "A/mm^2" },
    { "LV_RADIAL_WIDTH",           offsetof(Transformer, lv.rw),                FIELD_DOUBLE, "mm"     },
    { "LV_INNER_DIAMETER",         offsetof(Transformer, lv.di),                FIELD_DOUBLE, "mm"     },
    { "LV_OUTER_DIAMETER",         offsetof(Transformer, lv.do_),               FIELD_DOUBLE, "mm"     },
    { "LV_MEAN_TURN_LENGTH",       offsetof(Transformer, lv.Lmt),               FIELD_DOUBLE, "mm"     },
    { "LV_RESISTANCE_PER_PHASE",   offsetof(Transformer, lv.r),                 FIELD_DOUBLE, "mOhm"   },
    { "LV_COPPER_LOSS",            offsetof(Transformer, lv.pcu),               FIELD_DOUBLE, "kW"     },
    { "LV_CONDUCTOR_LENGTH",       offsetof(Transformer, lv.Lcu),               FIELD_DOUBLE, "m"      },
    { "LV_VOLUME",                 offsetof(Transformer, lv.Vcu),               FIELD_DOUBLE, "m^3"    },
    { "LV_WEIGHT",                 offsetof(Transformer, lv.Wcu),               FIELD_DOUBLE, "kg"     },

    /*==========================================================
      HV WINDING (STEPH) - Part 4
    ==========================================================*/
    { "HV_AVAILABLE_HEIGHT",       offsetof(Transformer, hv.ALW),               FIELD_DOUBLE, "mm"     },
    { "HV_TURNS",                  offsetof(Transformer, hv.T),                 FIELD_DOUBLE, "turns"  },
    { "HV_TURNS_AXIALLY",          offsetof(Transformer, hv.Ta),                FIELD_DOUBLE, "coils"  },
    { "HV_AXIAL_STRANDS",          offsetof(Transformer, hv.NstA),              FIELD_DOUBLE, "strands"},
    { "HV_RADIAL_STRANDS",         offsetof(Transformer, hv.NstR),              FIELD_DOUBLE, "strands"},
    { "HV_SPACE_PER_TURN",         offsetof(Transformer, hv.ALT),               FIELD_DOUBLE, "mm"     },
    { "HV_STRAND_WIDTH_BARE",      offsetof(Transformer, hv.stW),               FIELD_DOUBLE, "mm"     },
    { "HV_STRAND_THICKNESS_BARE",  offsetof(Transformer, hv.stT),               FIELD_DOUBLE, "mm"     },
    { "HV_SPACE_OCCUPIED_AXIALLY", offsetof(Transformer, hv.ALWx),              FIELD_DOUBLE, "mm"     },
    { "HV_AXIAL_SLACK",            offsetof(Transformer, hv.SlkAx),             FIELD_DOUBLE, "mm"     },
    { "HV_CONDUCTOR_AREA",         offsetof(Transformer, hv.a),                 FIELD_DOUBLE, "mm^2"   },
    { "HV_CURRENT_DENSITY",        offsetof(Transformer, hv.cd),                FIELD_DOUBLE, "A/mm^2" },
    { "HV_RADIAL_WIDTH",           offsetof(Transformer, hv.rw),                FIELD_DOUBLE, "mm"     },
    { "HV_INNER_DIAMETER",         offsetof(Transformer, hv.di),                FIELD_DOUBLE, "mm"     },
    { "HV_OUTER_DIAMETER",         offsetof(Transformer, hv.do_),               FIELD_DOUBLE, "mm"     },
    { "HV_MEAN_TURN_LENGTH",       offsetof(Transformer, hv.Lmt),               FIELD_DOUBLE, "m"      },
    { "HV_COPPER_LOSS",            offsetof(Transformer, hv.pcu),               FIELD_DOUBLE, "kW"     },
    { "HV_RESISTANCE_PER_PHASE",   offsetof(Transformer, hv.r),                 FIELD_DOUBLE, "Ohm"    },
    { "HV_CONDUCTOR_LENGTH",       offsetof(Transformer, hv.Lcu),               FIELD_DOUBLE, "m"      },
    { "HV_VOLUME",                 offsetof(Transformer, hv.Vcu),               FIELD_DOUBLE, "m^3"    },
    { "HV_WEIGHT",                 offsetof(Transformer, hv.Wcu),               FIELD_DOUBLE, "kg"     },

    /*==========================================================
      PERFORMANCE (HADASSAH) - Part 5
    ==========================================================*/
    { "COPPER_LOSS",               offsetof(Transformer, performance.pcuT),     FIELD_DOUBLE, "kW"     },
    { "TOTAL_LOSS",                offsetof(Transformer, performance.ptFL),     FIELD_DOUBLE, "kW"     },
    { "MAX_EFFICIENCY_LOAD",       offsetof(Transformer, performance.Ldmxef),   FIELD_DOUBLE, "kVA"    },
    { "MAX_EFFICIENCY",            offsetof(Transformer, performance.efmx),     FIELD_DOUBLE, "%"      },
    { "MEAN_TURN_LENGTH",          offsetof(Transformer, performance.Lmt),      FIELD_DOUBLE, "m"      },
    { "COIL_LENGTH",               offsetof(Transformer, performance.Lc),       FIELD_DOUBLE, "m"      },
    { "AMPERE_TURNS_PER_PHASE",    offsetof(Transformer, performance.AT),       FIELD_DOUBLE, "AT"     },
    { "ER",                        offsetof(Transformer, performance.Er),       FIELD_DOUBLE, "pu"     },
    { "EX",                        offsetof(Transformer, performance.Ex),       FIELD_DOUBLE, "pu"     },
    { "EZ",                        offsetof(Transformer, performance.Ez),       FIELD_DOUBLE, "pu"     },
    { "REG85",                     offsetof(Transformer, performance.Reg85),    FIELD_DOUBLE, "pu"     },
    { "REGUPF",                    offsetof(Transformer, performance.RegUPF),   FIELD_DOUBLE, "pu"     },

    /*==========================================================
      TANK (YONA) - Part 6
    ==========================================================*/
    { "CLEARANCE_LENGTH",          offsetof(Transformer, input.dL),             FIELD_DOUBLE, "m"      },
    { "CLEARANCE_WIDTH",           offsetof(Transformer, input.dB),             FIELD_DOUBLE, "m"      },
    { "CLEARANCE_HEIGHT",          offsetof(Transformer, input.dH),             FIELD_DOUBLE, "m"      },
    { "TUBE_DIAMETER",             offsetof(Transformer, input.Dct),            FIELD_DOUBLE, "m"      },
    { "TUBE_HEIGHT",               offsetof(Transformer, input.Hct),            FIELD_DOUBLE, "m"      },
    { "TANK_LENGTH",               offsetof(Transformer, tank.Lt),              FIELD_DOUBLE, "m"      },
    { "TANK_WIDTH",                offsetof(Transformer, tank.bt),              FIELD_DOUBLE, "m"      },
    { "TANK_HEIGHT",               offsetof(Transformer, tank.ht),              FIELD_DOUBLE, "m"      },
    { "TANK_VOLUME",               offsetof(Transformer, tank.Vt),              FIELD_DOUBLE, "m^3"    },
    { "SURFACE_AREA",              offsetof(Transformer, tank.St),              FIELD_DOUBLE, "m^2"    },
    { "TEMPERATURE_RISE",          offsetof(Transformer, tank.Tr),              FIELD_DOUBLE, "C"      },
    { "PERMISSIBLE_TEMP_RISE",     offsetof(Transformer, input.TRP),            FIELD_DOUBLE, "C"      },
    { "TUBE_AREA",                 offsetof(Transformer, tank.At),              FIELD_DOUBLE, "m^2"    },
    { "REQUIRED_AREA",             offsetof(Transformer, tank.CAt),             FIELD_DOUBLE, "m^2"    },
    { "COOLING_TUBES",             offsetof(Transformer, tank.Nt),              FIELD_INT,    "-"      },
    { "HV_COPPER_WEIGHT",          offsetof(Transformer, tank.Wcu1),            FIELD_DOUBLE, "kg"     },
    { "LV_COPPER_WEIGHT",          offsetof(Transformer, tank.Wcu2),            FIELD_DOUBLE, "kg"     },
    { "TOTAL_IRON_WEIGHT",         offsetof(Transformer, tank.Wiron),           FIELD_DOUBLE, "kg"     },
    { "TOTAL_WEIGHT",              offsetof(Transformer, tank.Wtot),            FIELD_DOUBLE, "kg"     },
    { "SPECIFIC_WEIGHT",           offsetof(Transformer, tank.KgPkva),          FIELD_DOUBLE, "kg/kVA" },

    /*==========================================================
      SOFTWARE DERIVED EXTENSIONS
    ==========================================================*/
    { "STEEL_WEIGHT",              offsetof(Transformer, tankDerived.Wsteel),   FIELD_DOUBLE, "kg"     },
    { "OIL_VOLUME",                offsetof(Transformer, tankDerived.Voil),     FIELD_DOUBLE, "m^3"    },
    { "OIL_WEIGHT",                offsetof(Transformer, tankDerived.Woil),     FIELD_DOUBLE, "kg"     },
    { "SHIPPING_WEIGHT",           offsetof(Transformer, tankDerived.Wship),    FIELD_DOUBLE, "kg"     }
};

static const int fieldTableSize = sizeof(fieldTable) / sizeof(fieldTable[0]);

static const FieldEntry *findField(const char *key) {
    for (int i = 0; i < fieldTableSize; i++)
        if (strcmp(fieldTable[i].key, key) == 0) return &fieldTable[i];
    return NULL;
}

static double getFieldValue(const Transformer *tx, const FieldEntry *f) {
    const char *base = (const char *)tx;
    if (f->type == FIELD_INT) return (double)(*(const int *)(base + f->offset));
    return *(const double *)(base + f->offset);
}

static void setFieldValue(Transformer *tx, const FieldEntry *f, double value) {
    char *base = (char *)tx;
    if (f->type == FIELD_INT) *(int *)(base + f->offset) = (int)value;
    else                      *(double *)(base + f->offset) = value;
}

static int keyWanted(const char *key, const char **keys, int nkeys) {
    if (keys == NULL) return 1;          // NULL => accept every recognized key
    for (int i = 0; i < nkeys; i++)
        if (strcmp(keys[i], key) == 0) return 1;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* tread: pull values FROM the txt file INTO the struct.                  */
/* Pass keys=NULL, nkeys=0 to load everything */
/* Pass a specific keys[] to only refresh those fields.                   */
/* ---------------------------------------------------------------------- */
int tread(Transformer *tx, const char *filename, const char **keys, int nkeys) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("tread: could not open config"); return -1; }

    char line[300];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *valueStr = eq + 1;

        if (!keyWanted(key, keys, nkeys)) continue;

        const FieldEntry *f = findField(key);
        if (!f) {
            printf("Warning: Unrecognized configuration key [%s] ignored.\n", key);
            continue;
        }
        setFieldValue(tx, f, strtod(valueStr, NULL));
    }
    fclose(fp);

    return 0;
}

/* ---------------------------------------------------------------------- */
/* twrite: push current struct values back OUT to the txt file.           */
/* Only the lines matching keys[] are rewritten in place; every other     */
/* line (comments, blanks, other keys) is preserved untouched. Any        */
/* requested key not already present is appended.                        */
/* ---------------------------------------------------------------------- */
int twrite(Transformer *tx, const char *filename, const char **keys, int nkeys) {
    /* NULL keys => operate over every entry in fieldTable, not just a subset */
    const char **effectiveKeys = keys;
    int effectiveN = nkeys;
    if (keys == NULL) {
        effectiveN = fieldTableSize;
        effectiveKeys = malloc(effectiveN * sizeof(char *));
        for (int i = 0; i < effectiveN; i++) effectiveKeys[i] = fieldTable[i].key;
    }

    FILE *fp = fopen(filename, "r");
    char **lines = NULL;
    int lineCount = 0, capacity = 0;

    if (fp) {
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            if (lineCount == capacity) {
                capacity = capacity ? capacity * 2 : 32;
                lines = realloc(lines, capacity * sizeof(char *));
            }
            buf[strcspn(buf, "\r\n")] = '\0';
            lines[lineCount++] = strdup(buf);
        }
        fclose(fp);
    }

    int *written = calloc(effectiveN > 0 ? effectiveN : 1, sizeof(int));

    for (int i = 0; i < lineCount; i++) {
        char temp[256];
        strncpy(temp, lines[i], sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';
        if (temp[0] == '\0' || temp[0] == '#') continue;

        char *eq = strchr(temp, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = temp;

        for (int k = 0; k < effectiveN; k++) {
            if (strcmp(effectiveKeys[k], key) == 0) {
                const FieldEntry *f = findField(key);
                if (f) {
                    double value = getFieldValue(tx, f);
                    char newLine[300];
                    if (f->type == FIELD_INT)
                        snprintf(newLine, sizeof(newLine), "%s=%d", key, (int)value);
                    else
                        snprintf(newLine, sizeof(newLine), "%s=%.6g", key, value);
                    free(lines[i]);
                    lines[i] = strdup(newLine);
                }
                written[k] = 1;
                break;
            }
        }
    }

    for (int k = 0; k < effectiveN; k++) {
        if (written[k]) continue;
        const FieldEntry *f = findField(effectiveKeys[k]);
        if (!f) {
            printf("Warning: twrite found no struct field for key [%s]\n", effectiveKeys[k]);
            continue;
        }
        double value = getFieldValue(tx, f);
        char newLine[300];
        if (f->type == FIELD_INT)
            snprintf(newLine, sizeof(newLine), "%s=%d", effectiveKeys[k], (int)value);
        else
            snprintf(newLine, sizeof(newLine), "%s=%.6g", effectiveKeys[k], value);

        if (lineCount == capacity) {
            capacity = capacity ? capacity * 2 : 32;
            lines = realloc(lines, capacity * sizeof(char *));
        }
        lines[lineCount++] = strdup(newLine);
    }
    free(written);
    if (keys == NULL) free((void *)effectiveKeys);   // free the temp array we built above

    FILE *out = fopen(filename, "w");
    if (!out) {
        perror("twrite: could not open config for writing");
        for (int i = 0; i < lineCount; i++) free(lines[i]);
        free(lines);
        return -1;
    }
    for (int i = 0; i < lineCount; i++) {
        fprintf(out, "%s\n", lines[i]);
        free(lines[i]);
    }
    free(lines);
    fclose(out);
    return 0;
}