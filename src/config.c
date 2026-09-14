#include "../include/main.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { INPUT_DOUBLE, INPUT_INT, INPUT_BOOL } InputType;
typedef struct { const char *key; size_t offset; InputType type; } InputField;

#define INPUT_FIELD(key, member, type) { key, offsetof(Transformer, input) + offsetof(DesignInputs, member), type }

static const InputField fields[] = {
    INPUT_FIELD("APPARENT_POWER", KVA, INPUT_DOUBLE),
    INPUT_FIELD("HV_VOLTAGE", HV, INPUT_DOUBLE),
    INPUT_FIELD("LV_VOLTAGE", LV, INPUT_DOUBLE),
    INPUT_FIELD("PHASES", Ph, INPUT_INT),
    INPUT_FIELD("FREQUENCY", f, INPUT_DOUBLE),
    INPUT_FIELD("VECTOR_CLOCK", vectorClock, INPUT_INT),
    INPUT_FIELD("TARGET_TEMP_RISE", TRP, INPUT_DOUBLE),
    INPUT_FIELD("REFERENCE_TEMPERATURE", referenceTemperatureC, INPUT_DOUBLE),
    INPUT_FIELD("AMBIENT_TEMPERATURE", ambientTemperatureC, INPUT_DOUBLE),
    INPUT_FIELD("COPPER_DENSITY", density_cu, INPUT_DOUBLE),
    INPUT_FIELD("STEEL_DENSITY", density_fe, INPUT_DOUBLE),
    INPUT_FIELD("OIL_DENSITY", density_oil, INPUT_DOUBLE),
    INPUT_FIELD("COPPER_RESISTIVITY_20C", copperResistivity20, INPUT_DOUBLE),
    INPUT_FIELD("COPPER_TEMPERATURE_CONSTANT", copperTemperatureConstant, INPUT_DOUBLE),
    INPUT_FIELD("CORE_STEP_FACTOR", k, INPUT_DOUBLE),
    INPUT_FIELD("EMF_VALUE_FACTOR", K, INPUT_DOUBLE),
    INPUT_FIELD("CORE_FLUX_DENSITY", Bm, INPUT_DOUBLE),
    INPUT_FIELD("AVERAGE_CURRENT_DENSITY", cdav, INPUT_DOUBLE),
    INPUT_FIELD("IRON_STACKING_FACTOR", ki, INPUT_DOUBLE),
    INPUT_FIELD("WINDOW_ASPECT_RATIO", windowAspectRatio, INPUT_DOUBLE),
    INPUT_FIELD("YOKE_AREA_FACTOR", yokeAreaFactor, INPUT_DOUBLE),
    INPUT_FIELD("YOKE_WIDTH_FACTOR", yokeWidthFactor, INPUT_DOUBLE),
    INPUT_FIELD("WINDOW_SPACE_MULTIPLIER", windowSpaceMultiplier, INPUT_DOUBLE),
    INPUT_FIELD("DIMENSION_ROUNDING", dimensionRoundingM, INPUT_DOUBLE),
    INPUT_FIELD("CORE_LOSS_REFERENCE_FLUX", coreLossReferenceFluxT, INPUT_DOUBLE),
    INPUT_FIELD("CORE_LOSS_REFERENCE", coreLossReferenceWKg, INPUT_DOUBLE),
    INPUT_FIELD("YOKE_LOSS_REFERENCE_FLUX", yokeLossReferenceFluxT, INPUT_DOUBLE),
    INPUT_FIELD("YOKE_LOSS_REFERENCE", yokeLossReferenceWKg, INPUT_DOUBLE),
    INPUT_FIELD("LOSS_CURVE_EXPONENT", lossCurveExponent, INPUT_DOUBLE),
    INPUT_FIELD("IRON_LOSS_BUILD_FACTOR", ironLossBuildFactor, INPUT_DOUBLE),
    INPUT_FIELD("CORE_AT_PER_METER", coreATPerMeter, INPUT_DOUBLE),
    INPUT_FIELD("YOKE_AT_PER_METER", yokeATPerMeter, INPUT_DOUBLE),
    INPUT_FIELD("EXCITATION_BUILD_FACTOR", excitationBuildFactor, INPUT_DOUBLE),
    INPUT_FIELD("CONDUCTOR_INSULATION", conductorInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("COPPER_STRAY_LOSS_FACTOR", copperStrayLossFactor, INPUT_DOUBLE),
    INPUT_FIELD("LV_TURNS_RADIALLY", lvTurnsRadially, INPUT_INT),
    INPUT_FIELD("LV_PARALLEL_STRANDS", lvParallelStrands, INPUT_INT),
    INPUT_FIELD("LV_AXIAL_STRANDS", lvAxialStrands, INPUT_INT),
    INPUT_FIELD("LV_STRAND_WIDTH_BARE", lvStrandWidthMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_STRAND_THICKNESS_BARE", lvStrandThicknessMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_EDGE_FACTOR", lvEdgeFactor, INPUT_DOUBLE),
    INPUT_FIELD("LV_WINDING_HEIGHT_FRACTION", lvWindingHeightFraction, INPUT_DOUBLE),
    INPUT_FIELD("LV_INTER_TURN_INSULATION", lvInterTurnInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_END_INSULATION", lvEndInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_RADIAL_INSULATION", lvRadialInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("CORE_TO_LV_OIL_DUCT", coreToLvOilDuctMm, INPUT_DOUBLE),
    INPUT_FIELD("CORE_TO_LV_CYLINDER", coreToLvCylinderMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_FORMER_TO_WINDING_DUCT", lvFormerToWindingDuctMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_COILS", hvCoils, INPUT_INT),
    INPUT_FIELD("HV_AXIAL_STRANDS", hvAxialStrands, INPUT_INT),
    INPUT_FIELD("HV_RADIAL_STRANDS", hvRadialStrands, INPUT_INT),
    INPUT_FIELD("HV_STRAND_WIDTH_BARE", hvStrandWidthMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_STRAND_THICKNESS_BARE", hvStrandThicknessMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_EDGE_FACTOR", hvEdgeFactor, INPUT_DOUBLE),
    INPUT_FIELD("HV_WINDING_HEIGHT_FRACTION", hvWindingHeightFraction, INPUT_DOUBLE),
    INPUT_FIELD("HV_INTER_COIL_INSULATION", hvInterCoilInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_END_RING", hvEndRingMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_END_INSULATION", hvEndInsulationMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_TO_HV_OIL_DUCT", lvToHvOilDuctMm, INPUT_DOUBLE),
    INPUT_FIELD("LV_TO_HV_CYLINDER", lvToHvCylinderMm, INPUT_DOUBLE),
    INPUT_FIELD("HV_FORMER_TO_WINDING_DUCT", hvFormerToWindingDuctMm, INPUT_DOUBLE),
    INPUT_FIELD("TUBE_DIAMETER", Dct, INPUT_DOUBLE),
    INPUT_FIELD("TUBE_HEIGHT", Hct, INPUT_DOUBLE),
    INPUT_FIELD("CLEARANCE_LENGTH", dL, INPUT_DOUBLE),
    INPUT_FIELD("CLEARANCE_WIDTH", dB, INPUT_DOUBLE),
    INPUT_FIELD("CLEARANCE_HEIGHT", dH, INPUT_DOUBLE),
    INPUT_FIELD("PLAIN_TANK_DISSIPATION", plainTankDissipation, INPUT_DOUBLE),
    INPUT_FIELD("TUBE_COEFFICIENT", tubeCoefficient, INPUT_DOUBLE),
    INPUT_FIELD("TUBE_EFFECTIVENESS", tubeEffectiveness, INPUT_DOUBLE),
    INPUT_FIELD("TANK_PLATE_THICKNESS", tankPlateThicknessM, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_BM_MIN", optimizerBmMin, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_BM_MAX", optimizerBmMax, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_CURRENT_DENSITY_MIN", optimizerCurrentDensityMin, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_CURRENT_DENSITY_MAX", optimizerCurrentDensityMax, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_ASPECT_RATIO_MIN", optimizerAspectRatioMin, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_ASPECT_RATIO_MAX", optimizerAspectRatioMax, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_ACTUAL_CURRENT_DENSITY_MIN", optimizerActualCurrentDensityMin, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_ACTUAL_CURRENT_DENSITY_MAX", optimizerActualCurrentDensityMax, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MIN_AXIAL_SLACK_MM", optimizerMinAxialSlackMm, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MIN_ADJACENT_CLEARANCE_MM", optimizerMinAdjacentClearanceMm, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_TEMPERATURE_MARGIN_C", optimizerTemperatureMarginC, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MIN_EFFICIENCY_PERCENT", optimizerMinEfficiencyPercent, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MAX_SPECIFIC_MASS_KG_KVA", optimizerMaxSpecificMassKgKva, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MAX_NO_LOAD_CURRENT_PERCENT", optimizerMaxNoLoadCurrentPercent, INPUT_DOUBLE),
    INPUT_FIELD("OPTIMIZER_MAX_TANK_VOLUME_M3", optimizerMaxTankVolumeM3, INPUT_DOUBLE),
    INPUT_FIELD("COPPER_COST_INDEX", copperCostIndex, INPUT_DOUBLE),
    INPUT_FIELD("CORE_STEEL_COST_INDEX", coreSteelCostIndex, INPUT_DOUBLE),
    INPUT_FIELD("TANK_STEEL_COST_INDEX", tankSteelCostIndex, INPUT_DOUBLE),
    INPUT_FIELD("OIL_COST_INDEX", oilCostIndex, INPUT_DOUBLE),
    INPUT_FIELD("AUTOMATIC_CONDUCTOR_SIZING", automaticConductorSizing, INPUT_BOOL)
};

static int textEquals(const char *left, const char *right)
{
    while (*left && *right) {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) return 0;
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return text;
}

void setDefaultConfiguration(Transformer *tx)
{
    memset(tx, 0, sizeof(*tx));
    DesignInputs *in = &tx->input;
    in->KVA = 630.0; in->HV = 11000.0; in->LV = 415.0; in->Ph = 3; in->f = 50.0;
    in->hvConnection = CONNECTION_DELTA; in->lvConnection = CONNECTION_STAR;
    in->vectorClock = 11; strcpy(in->cooling, "ONAN");
    in->TRP = 50.0; in->referenceTemperatureC = 75.0; in->ambientTemperatureC = 30.0;
    in->density_cu = 8960.0; in->density_fe = 7850.0; in->density_oil = 860.0;
    in->copperResistivity20 = 0.01724; in->copperTemperatureConstant = 235.0;
    in->k = 0.6; in->K = 0.6; in->Bm = 1.6; in->cdav = 2.6; in->ki = 0.9;
    in->windowAspectRatio = 3.0; in->yokeAreaFactor = 1.15; in->yokeWidthFactor = 0.9;
    in->windowSpaceMultiplier = 1.15; in->dimensionRoundingM = 0.001;
    in->coreLossReferenceFluxT = 1.6; in->coreLossReferenceWKg = 1.4;
    in->yokeLossReferenceFluxT = 1.33; in->yokeLossReferenceWKg = 1.02;
    in->lossCurveExponent = 2.0; in->ironLossBuildFactor = 1.0;
    in->coreATPerMeter = 200.0; in->yokeATPerMeter = 110.0; in->excitationBuildFactor = 1.15;
    in->conductorInsulationMm = 0.4; in->copperStrayLossFactor = 1.05;
    in->lvTurnsRadially = 7; in->lvParallelStrands = 18; in->lvAxialStrands = 6;
    in->lvStrandWidthMm = 20.0; in->lvStrandThicknessMm = 1.0; in->lvEdgeFactor = 0.9702;
    in->lvWindingHeightFraction = 0.8; in->lvInterTurnInsulationMm = 2.0;
    in->lvEndInsulationMm = 100.0; in->lvRadialInsulationMm = 1.8;
    in->coreToLvOilDuctMm = 5.0; in->coreToLvCylinderMm = 3.0; in->lvFormerToWindingDuctMm = 5.0;
    in->hvCoils = 12; in->hvAxialStrands = 4; in->hvRadialStrands = 28;
    in->hvStrandWidthMm = 8.0; in->hvStrandThicknessMm = 1.0; in->hvEdgeFactor = 0.98;
    in->hvWindingHeightFraction = 0.7; in->hvInterCoilInsulationMm = 6.0;
    in->hvEndRingMm = 30.0; in->hvEndInsulationMm = 100.0;
    in->lvToHvOilDuctMm = 5.0; in->lvToHvCylinderMm = 6.0; in->hvFormerToWindingDuctMm = 5.0;
    in->Dct = 0.05; in->Hct = 1.25; in->dL = 0.14; in->dB = 0.18; in->dH = 0.5;
    in->plainTankDissipation = 12.5; in->tubeCoefficient = 6.5; in->tubeEffectiveness = 1.35;
    in->tankPlateThicknessM = 0.008;
    in->optimizerBmMin = 1.4; in->optimizerBmMax = 1.7;
    in->optimizerCurrentDensityMin = 2.3; in->optimizerCurrentDensityMax = 3.2;
    in->optimizerAspectRatioMin = 2.5; in->optimizerAspectRatioMax = 4.0;
    in->optimizerActualCurrentDensityMin = 2.15; in->optimizerActualCurrentDensityMax = 3.6;
    in->optimizerMinAxialSlackMm = 0.0; in->optimizerMinAdjacentClearanceMm = 0.0;
    in->optimizerTemperatureMarginC = 0.5;
    in->optimizerMinEfficiencyPercent = 98.0; in->optimizerMaxSpecificMassKgKva = 4.0;
    in->optimizerMaxNoLoadCurrentPercent = 1.0; in->optimizerMaxTankVolumeM3 = 1.5;
    in->copperCostIndex = 8.0; in->coreSteelCostIndex = 2.4;
    in->tankSteelCostIndex = 1.2; in->oilCostIndex = 1.0;
    in->automaticConductorSizing = false;
}

int applyConfigurationValue(Transformer *tx, const char *key, const char *rawValue)
{
    char valueBuffer[128];
    snprintf(valueBuffer, sizeof(valueBuffer), "%s", rawValue);
    char *comment = strchr(valueBuffer, '#');
    if (comment) *comment = '\0';
    char *value = trim(valueBuffer);

    if (textEquals(key, "HV_CONNECTION")) return connectionFromString(value, &tx->input.hvConnection);
    if (textEquals(key, "LV_CONNECTION")) return connectionFromString(value, &tx->input.lvConnection);
    if (textEquals(key, "COOLING_METHOD")) {
        snprintf(tx->input.cooling, sizeof(tx->input.cooling), "%s", value);
        return 0;
    }

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (!textEquals(key, fields[i].key)) continue;
        char *base = (char *)tx;
        char *end = NULL;
        double number = strtod(value, &end);
        if (end == value) return -1;
        if (fields[i].type == INPUT_DOUBLE) *(double *)(base + fields[i].offset) = number;
        else if (fields[i].type == INPUT_INT) *(int *)(base + fields[i].offset) = (int)number;
        else *(bool *)(base + fields[i].offset) = number != 0.0;
        return 0;
    }
    return 1;
}

int loadConfiguration(Transformer *tx, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) return -1;
    char line[512];
    int lineNumber = 0;
    while (fgets(line, sizeof(line), file)) {
        lineNumber++;
        char *content = trim(line);
        if (*content == '\0' || *content == '#') continue;
        char *equals = strchr(content, '=');
        if (!equals) {
            fprintf(stderr, "config:%d: expected KEY=VALUE\n", lineNumber);
            fclose(file);
            return -2;
        }
        *equals = '\0';
        char *key = trim(content);
        int result = applyConfigurationValue(tx, key, trim(equals + 1));
        if (result != 0) {
            fprintf(stderr, "config:%d: invalid or unknown key '%s'\n", lineNumber, key);
            fclose(file);
            return -3;
        }
    }
    fclose(file);
    return 0;
}
