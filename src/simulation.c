#include "../include/transformer_design.h"
#include "../include/frame_design.h"
#include "../include/no_load_current_design.h"
#include "../include/lv_windings_design.h"
#include "../include/hv_windings_design.h"
#include "../include/performance_design.h"
#include "../include/tank_design.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
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

const char *connectionName(ConnectionKind connection)
{
    return connection == CONNECTION_DELTA ? "Delta" : "Star";
}

const char *connectionShortName(ConnectionKind connection)
{
    return connection == CONNECTION_DELTA ? "D" : "Y";
}

int connectionFromString(const char *text, ConnectionKind *connection)
{
    if (textEquals(text, "STAR") || textEquals(text, "Y")) {
        *connection = CONNECTION_STAR;
        return 0;
    }
    if (textEquals(text, "DELTA") || textEquals(text, "D")) {
        *connection = CONNECTION_DELTA;
        return 0;
    }
    return -1;
}

const char *runModeName(RunMode mode)
{
    if (mode == RUN_EXPLORE) return "explore";
    if (mode == RUN_OPTIMIZE) return "optimize";
    return "nominal";
}

const char *statusName(EvaluationStatus status)
{
    if (status == STATUS_PASS) return "PASS";
    if (status == STATUS_WARN) return "WARN";
    if (status == STATUS_FAIL) return "FAIL";
    return "INFO";
}

double windingPhaseVoltage(double lineVoltage, ConnectionKind connection)
{
    return connection == CONNECTION_DELTA ? lineVoltage : lineVoltage / sqrt(3.0);
}

double windingPhaseCurrent(double kva, double lineVoltage, int phases, ConnectionKind connection)
{
    const double phaseVoltage = windingPhaseVoltage(lineVoltage, connection);
    return kva * 1000.0 / (phases * phaseVoltage);
}

static int validateInputs(const DesignInputs *in)
{
    if (in->KVA <= 0.0 || in->HV <= 0.0 || in->LV <= 0.0 || in->f <= 0.0 || in->Ph < 1)
        return -1;
    if (in->k <= 0.0 || in->K <= 0.0 || in->Bm <= 0.0 || in->cdav <= 0.0 || in->ki <= 0.0)
        return -2;
    if (in->windowAspectRatio <= 0.0 || in->lvTurnsRadially < 1 || in->lvParallelStrands < 1 ||
        in->lvAxialStrands < 1 || in->hvCoils < 3 || in->hvAxialStrands < 1 ||
        in->hvRadialStrands < 1)
        return -3;
    if (in->lvParallelStrands % in->lvAxialStrands != 0) return -4;
    if (in->optimizerBmMin <= 0.0 || in->optimizerBmMin >= in->optimizerBmMax ||
        in->optimizerCurrentDensityMin <= 0.0 || in->optimizerCurrentDensityMin >= in->optimizerCurrentDensityMax ||
        in->optimizerAspectRatioMin <= 0.0 || in->optimizerAspectRatioMin >= in->optimizerAspectRatioMax ||
        in->optimizerActualCurrentDensityMin <= 0.0 || in->optimizerActualCurrentDensityMin >= in->optimizerActualCurrentDensityMax ||
        in->optimizerMinAxialSlackMm < 0.0 || in->optimizerMinAdjacentClearanceMm < 0.0 ||
        in->optimizerTemperatureMarginC < 0.0 || in->optimizerMinEfficiencyPercent <= 0.0 ||
        in->optimizerMinEfficiencyPercent >= 100.0 || in->optimizerMaxSpecificMassKgKva <= 0.0 ||
        in->optimizerMaxNoLoadCurrentPercent <= 0.0 || in->optimizerMaxTankVolumeM3 <= 0.0)
        return -5;
    return 0;
}

static int highestRequestedSection(unsigned sections)
{
    for (int index = 5; index >= 0; index--)
        if (sections & (1u << index)) return index;
    return 0;
}

int runSimulation(Transformer *tx, unsigned requestedSections)
{
    int validation = validateInputs(&tx->input);
    if (validation != 0) return validation;
    if (requestedSections == 0) requestedSections = SECTION_ALL;

    memset(&tx->magneticFrame, 0, sizeof(tx->magneticFrame));
    memset(&tx->noLoadCurrent, 0, sizeof(tx->noLoadCurrent));
    memset(&tx->lv, 0, sizeof(tx->lv));
    memset(&tx->hv, 0, sizeof(tx->hv));
    memset(&tx->performance, 0, sizeof(tx->performance));
    memset(&tx->tank, 0, sizeof(tx->tank));
    memset(&tx->tankDerived, 0, sizeof(tx->tankDerived));
    tx->evaluationCount = 0;
    tx->requestedSections = requestedSections;
    tx->completedSections = 0;

    int highest = highestRequestedSection(requestedSections);
    designFrame(tx); tx->completedSections |= SECTION_FRAME;
    if (highest >= 1) { designNoLoadCurrent(tx); tx->completedSections |= SECTION_NO_LOAD; }
    if (highest >= 2) { designLVWindings(tx); tx->completedSections |= SECTION_LV; }
    if (highest >= 3) { designHVWindings(tx); tx->completedSections |= SECTION_HV; }
    if (highest >= 4) { designPerformance(tx); tx->completedSections |= SECTION_PERFORMANCE; }
    if (highest >= 5) { designTank(tx); tx->completedSections |= SECTION_TANK; }
    evaluateTransformer(tx);
    return 0;
}

static double interpolate(const double *ratings, const double *values, int count, double rating)
{
    if (rating <= ratings[0]) return values[0] * rating / ratings[0];
    for (int i = 1; i < count; i++) {
        if (rating <= ratings[i]) {
            double fraction = (rating - ratings[i - 1]) / (ratings[i] - ratings[i - 1]);
            return values[i - 1] + fraction * (values[i] - values[i - 1]);
        }
    }
    return values[count - 1] * rating / ratings[count - 1];
}

static void localBenchmark(double kva, double *noLoadW, double *loadW,
                           double *totalW, double *impedancePercent)
{
    static const double ratings[] = {100.0, 200.0, 315.0, 630.0, 1000.0};
    static const double noLoad[] = {145.0, 310.0, 440.0, 680.0, 940.0};
    static const double load[] = {1250.0, 2375.0, 3250.0, 5600.0, 9000.0};
    static const double total[] = {1395.0, 2685.0, 3690.0, 6280.0, 9940.0};
    *noLoadW = interpolate(ratings, noLoad, 5, kva);
    *loadW = interpolate(ratings, load, 5, kva);
    *totalW = interpolate(ratings, total, 5, kva);
    *impedancePercent = kva <= 630.0 ? 4.0 : 4.0 + (kva - 630.0) / 370.0 * 2.0;
    if (*impedancePercent > 6.0) *impedancePercent = 6.0;
}

static void addEvaluation(Transformer *tx, const char *id, const char *label,
                          double value, const char *unit, double low, double high,
                          EvaluationStatus status, const char *source,
                          const char *meaning, const char *verdict)
{
    if (tx->evaluationCount >= MAX_EVALUATIONS) return;
    Evaluation *e = &tx->evaluations[tx->evaluationCount++];
    snprintf(e->id, sizeof(e->id), "%s", id);
    snprintf(e->label, sizeof(e->label), "%s", label);
    snprintf(e->unit, sizeof(e->unit), "%s", unit);
    snprintf(e->source, sizeof(e->source), "%s", source);
    snprintf(e->meaning, sizeof(e->meaning), "%s", meaning);
    snprintf(e->verdict, sizeof(e->verdict), "%s", verdict);
    e->value = value; e->limitLow = low; e->limitHigh = high; e->status = status;
}

static EvaluationStatus upperLimitStatus(double value, double limit)
{
    if (value <= limit) return STATUS_PASS;
    if (value <= limit * 1.10) return STATUS_WARN;
    return STATUS_FAIL;
}

void evaluateTransformer(Transformer *tx)
{
    tx->evaluationCount = 0;
    if (tx->completedSections & SECTION_FRAME) {
        EvaluationStatus ratioStatus = tx->magneticFrame.windowRatio >= 2.5 &&
            tx->magneticFrame.windowRatio <= 4.0 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "window_ratio", "Core window proportion", tx->magneticFrame.windowRatio,
            "ratio", 2.5, 4.0, ratioStatus, "Academic design rule",
            "L/(D-d) checks whether the magnetic window is sensibly proportioned.",
            ratioStatus == STATUS_PASS ? "Inside the 2.5 to 4.0 design band." : "Outside the accepted geometry band.");
    }
    if (tx->completedSections & SECTION_NO_LOAD) {
        double value = tx->noLoadCurrent.I0byI2;
        EvaluationStatus status = value >= 0.5 && value <= 1.0 ? STATUS_PASS :
            (value >= 0.4 && value <= 1.2 ? STATUS_WARN : STATUS_FAIL);
        addEvaluation(tx, "no_load_current", "No-load current", value, "%", 0.5, 1.0, status,
            "Academic design rule", "Excitation current as a percentage of rated LV phase current.",
            status == STATUS_PASS ? "Inside the textbook 0.5 to 1.0 percent band." :
            (status == STATUS_WARN ? "Close to, but outside, the textbook band." :
            "Outside the textbook band; review flux, core data and excitation assumptions."));
    }
    if (tx->completedSections & SECTION_LV) {
        EvaluationStatus density = tx->lv.cd >= 2.3 && tx->lv.cd <= 3.5 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "lv_current_density", "LV current density", tx->lv.cd, "A/mm2", 2.3, 3.5,
            density, "Academic design rule", "Copper loading determines conductor heating and material use.",
            density == STATUS_PASS ? "Within the selected transformer-design band." : "Outside 2.3 to 3.5 A/mm2; resize the conductor.");
        EvaluationStatus slack = tx->lv.SlkAx >= 7.0 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "lv_slack", "LV axial slack", tx->lv.SlkAx, "mm", 7.0, 1e9, slack,
            "Academic design rule", "Remaining axial allowance for insulation and assembly tolerance.",
            slack == STATUS_PASS ? "At least 7 mm remains." : "Insufficient axial allowance.");
    }
    if (tx->completedSections & SECTION_HV) {
        EvaluationStatus density = tx->hv.cd >= 2.3 && tx->hv.cd <= 3.5 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "hv_current_density", "HV current density", tx->hv.cd, "A/mm2", 2.3, 3.5,
            density, "Academic design rule", "Copper loading determines conductor heating and material use.",
            density == STATUS_PASS ? "Within the selected transformer-design band." : "Outside 2.3 to 3.5 A/mm2; resize the conductor.");
        EvaluationStatus slack = tx->hv.SlkAx >= 7.0 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "hv_slack", "HV axial slack", tx->hv.SlkAx, "mm", 7.0, 1e9, slack,
            "Academic design rule", "Remaining height after coils, end ring and insulation are placed.",
            slack == STATUS_PASS ? "At least 7 mm remains." : "The selected coil stack does not fit safely.");
        double clearance = tx->magneticFrame.D * 1000.0 - tx->hv.do_;
        EvaluationStatus clearanceStatus = clearance >= 15.0 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "inter_winding_clearance", "Adjacent HV winding clearance", clearance, "mm",
            15.0, 1e9, clearanceStatus, "Academic design rule",
            "Clearance between adjacent HV winding envelopes on neighbouring limbs.",
            clearanceStatus == STATUS_PASS ? "Meets the desired 15 mm minimum." : "Winding envelopes are too close.");
    }
    if (tx->completedSections & SECTION_PERFORMANCE) {
        double noLoadLimit, loadLimit, totalLimit, impedanceTarget;
        localBenchmark(tx->input.KVA, &noLoadLimit, &loadLimit, &totalLimit, &impedanceTarget);
        double noLoadW = tx->magneticFrame.Pi * 1000.0;
        double loadW = tx->performance.pcuT * 1000.0;
        double totalW = tx->performance.ptFL * 1000.0;
        EvaluationStatus s1 = upperLimitStatus(noLoadW, noLoadLimit);
        EvaluationStatus s2 = upperLimitStatus(loadW, loadLimit);
        EvaluationStatus s3 = upperLimitStatus(totalW, totalLimit);
        addEvaluation(tx, "kplc_no_load_loss", "No-load loss vs local benchmark", noLoadW, "W", 0.0,
            noLoadLimit, s1, "Kenya Power 2025, Table 6",
            "Core loss compared with the interpolated 11/0.420 kV utility benchmark.",
            s1 == STATUS_PASS ? "At or below the local benchmark." : "Above the local benchmark; improve core material or flux selection.");
        addEvaluation(tx, "kplc_load_loss", "Load loss at reference temperature", loadW, "W", 0.0,
            loadLimit, s2, "Kenya Power 2025, Table 6",
            "Copper and stray loss corrected to the configured reference temperature.",
            s2 == STATUS_PASS ? "At or below the local benchmark." : "Above the local benchmark; review conductor area and current density.");
        addEvaluation(tx, "kplc_total_loss", "Total loss vs local benchmark", totalW, "W", 0.0,
            totalLimit, s3, "Kenya Power 2025, Table 5",
            "Full-load loss at unity power factor, compared with the local rating curve.",
            s3 == STATUS_PASS ? "Meets the local total-loss benchmark." : "Does not meet the local total-loss benchmark.");

        double impedance = tx->performance.Ez * 100.0;
        double deviation = fabs(impedance - impedanceTarget) / impedanceTarget;
        EvaluationStatus impedanceStatus = deviation <= 0.10 ? STATUS_PASS :
            (deviation <= 0.20 ? STATUS_WARN : STATUS_FAIL);
        char impedanceVerdict[220];
        snprintf(impedanceVerdict, sizeof(impedanceVerdict),
            "Calculated %.2f%% against a %.2f%% local benchmark target.", impedance, impedanceTarget);
        addEvaluation(tx, "kplc_impedance", "Short-circuit impedance", impedance, "%",
            impedanceTarget * 0.9, impedanceTarget * 1.1, impedanceStatus,
            "Kenya Power 2025, Table 5", "Impedance affects fault current and voltage regulation.", impedanceVerdict);
        addEvaluation(tx, "full_load_efficiency", "Full-load efficiency at unity PF",
            tx->performance.cases[0].efficiency, "%", 0.0, 100.0, STATUS_INFO,
            "Calculated result", "Useful output divided by electrical input at rated load.",
            "Higher is better; loss limits provide the formal local comparison.");
    }
    if (tx->completedSections & SECTION_TANK) {
        EvaluationStatus thermal = tx->tank.TrWithTubes <= tx->input.TRP + 0.05 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "temperature_rise", "Calculated oil temperature rise", tx->tank.TrWithTubes,
            "C", 0.0, tx->input.TRP, thermal, "Configured thermal target / IEC 60076-2 context",
            "Estimated steady rise after the calculated cooling tubes are included.",
            thermal == STATUS_PASS ? "Cooling area holds the estimate within the target." : "Cooling surface is insufficient.");
        EvaluationStatus oil = tx->tankDerived.Voil > 0.0 ? STATUS_PASS : STATUS_FAIL;
        addEvaluation(tx, "oil_volume", "Usable oil volume", tx->tankDerived.Voil, "m3", 0.0, 1e9,
            oil, "Calculated geometry", "Tank volume remaining after active-material displacement.",
            oil == STATUS_PASS ? "Positive oil volume is available." : "Active materials exceed the tank envelope.");
    }
}
