#include "../include/main.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int textEquals(const char *left, const char *right)
{
    while (*left && *right) {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) return 0;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static void usage(const char *program)
{
    printf("Usage: %s [options]\n", program);
    printf("  --mode nominal|explore|optimize\n");
    printf("  --config PATH\n");
    printf("  --sections frame,no-load,lv,hv,performance,tank|all\n");
    printf("  --set KEY=VALUE       Repeat for temporary overrides\n");
    printf("  --output PATH         Human-readable report\n");
    printf("  --json PATH           Machine-readable report\n");
    printf("  --stdout-json         Emit JSON for dashboard integration\n");
    printf("  --no-files            Do not update report files\n");
}

static RunMode parseMode(const char *text)
{
    if (textEquals(text, "explore")) return RUN_EXPLORE;
    if (textEquals(text, "optimize")) return RUN_OPTIMIZE;
    return RUN_NOMINAL;
}

static unsigned parseSections(const char *text)
{
    if (textEquals(text, "all")) return SECTION_ALL;
    char buffer[160];
    snprintf(buffer, sizeof(buffer), "%s", text);
    unsigned result = 0;
    for (char *token = strtok(buffer, ","); token; token = strtok(NULL, ",")) {
        while (isspace((unsigned char)*token)) token++;
        if (textEquals(token, "frame")) result |= SECTION_FRAME;
        else if (textEquals(token, "no-load") || textEquals(token, "noload")) result |= SECTION_NO_LOAD;
        else if (textEquals(token, "lv")) result |= SECTION_LV;
        else if (textEquals(token, "hv")) result |= SECTION_HV;
        else if (textEquals(token, "performance")) result |= SECTION_PERFORMANCE;
        else if (textEquals(token, "tank")) result |= SECTION_TANK;
    }
    return result;
}

static int fileExists(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    fclose(file);
    return 1;
}

int main(int argc, char **argv)
{
    const char *configPath = "data/config.txt";
    const char *outputPath = "data/output.txt";
    const char *jsonPath = "data/output.json";
    RunMode mode = RUN_NOMINAL;
    unsigned sections = SECTION_ALL;
    int stdoutJson = 0;
    int writeFiles = 1;

    for (int i = 1; i < argc; i++) {
        if (textEquals(argv[i], "--help") || textEquals(argv[i], "-h")) { usage(argv[0]); return 0; }
        if (textEquals(argv[i], "--mode") && i + 1 < argc) mode = parseMode(argv[++i]);
        else if (textEquals(argv[i], "--config") && i + 1 < argc) configPath = argv[++i];
        else if (textEquals(argv[i], "--sections") && i + 1 < argc) sections = parseSections(argv[++i]);
        else if (textEquals(argv[i], "--output") && i + 1 < argc) outputPath = argv[++i];
        else if (textEquals(argv[i], "--json") && i + 1 < argc) jsonPath = argv[++i];
        else if (textEquals(argv[i], "--stdout-json")) stdoutJson = 1;
        else if (textEquals(argv[i], "--no-files")) writeFiles = 0;
        else if (textEquals(argv[i], "--set") && i + 1 < argc) i++;
    }

    Transformer tx;
    setDefaultConfiguration(&tx);
    if (!fileExists(configPath) && strcmp(configPath, "data/config.txt") == 0 && fileExists("../data/config.txt"))
        configPath = "../data/config.txt";
    if (loadConfiguration(&tx, configPath) != 0) {
        fprintf(stderr, "Unable to load configuration: %s\n", configPath);
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        if (!textEquals(argv[i], "--set") || i + 1 >= argc) continue;
        char override[256];
        snprintf(override, sizeof(override), "%s", argv[++i]);
        char *equals = strchr(override, '=');
        if (!equals) {
            fprintf(stderr, "Invalid override '%s'; expected KEY=VALUE\n", override);
            return EXIT_FAILURE;
        }
        *equals = '\0';
        if (applyConfigurationValue(&tx, override, equals + 1) != 0) {
            fprintf(stderr, "Invalid override key or value: %s\n", override);
            return EXIT_FAILURE;
        }
    }

    tx.runMode = mode;
    if (mode == RUN_NOMINAL) sections = SECTION_ALL;
    if (sections == 0) {
        fprintf(stderr, "No valid design sections selected.\n");
        return EXIT_FAILURE;
    }
    if (runSimulation(&tx, sections) != 0) {
        fprintf(stderr, "Simulation failed: check ratings, factors and winding selections.\n");
        return EXIT_FAILURE;
    }

    OptimizationSet optimization;
    OptimizationSet *optimizationPtr = NULL;
    if (mode == RUN_OPTIMIZE) {
        if (runOptimization(&tx, &optimization) != 0) {
            fprintf(stderr, "Optimization found no design satisfying the hard constraints.\n");
        }
        optimizationPtr = &optimization;
    }

    if (writeFiles) {
        if (writeTextReport(&tx, optimizationPtr, outputPath) != 0 ||
            writeJsonReport(&tx, optimizationPtr, jsonPath) != 0) {
            fprintf(stderr, "Unable to write one or more output files.\n");
            return EXIT_FAILURE;
        }
    }

    if (stdoutJson) writeJsonStream(&tx, optimizationPtr, stdout);
    else printTransformerResults(&tx);
    return EXIT_SUCCESS;
}
