#include "../include/transformer_design.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEARCH_DESIGNS 512
#define SEARCH_DESIGNS (7 * 7 * 6)

static int isSafeFeasible(const Transformer *tx)
{
    const double clearance = tx->magneticFrame.D * 1000.0 - tx->hv.do_;
    return isfinite(tx->performance.ptFL) && isfinite(tx->tank.Wtot) &&
        tx->lv.cd >= 2.3 && tx->lv.cd <= 3.5 &&
        tx->hv.cd >= 2.3 && tx->hv.cd <= 3.5 &&
        tx->lv.SlkAx >= 7.0 && tx->hv.SlkAx >= 7.0 &&
        tx->hv.endCoilTurns > 0.0 && clearance >= 15.0 &&
        tx->tank.TrWithTubes <= tx->input.TRP + 0.05 &&
        tx->tankDerived.Voil > 0.0;
}

static int benchmarkPassCount(const Transformer *tx)
{
    int count = 0;
    for (int i = 0; i < tx->evaluationCount; i++) {
        const Evaluation *e = &tx->evaluations[i];
        if (strncmp(e->id, "kplc_", 5) == 0 && e->status == STATUS_PASS) count++;
    }
    return count;
}

static int dominates(const OptimizationCandidate *a, const OptimizationCandidate *b)
{
    const int noWorse = a->totalLossW <= b->totalLossW &&
        a->activeMassKg <= b->activeMassKg &&
        a->materialCostIndex <= b->materialCostIndex;
    const int strictlyBetter = a->totalLossW < b->totalLossW ||
        a->activeMassKg < b->activeMassKg ||
        a->materialCostIndex < b->materialCostIndex;
    return noWorse && strictlyBetter;
}

static int compareBalance(const void *left, const void *right)
{
    const OptimizationCandidate *a = (const OptimizationCandidate *)left;
    const OptimizationCandidate *b = (const OptimizationCandidate *)right;
    if (a->balanceScore < b->balanceScore) return -1;
    if (a->balanceScore > b->balanceScore) return 1;
    return (a->serialNumber > b->serialNumber) - (a->serialNumber < b->serialNumber);
}

static void summarize(const Transformer *tx, int serial, int calculated, OptimizationCandidate *c)
{
    memset(c, 0, sizeof(*c));
    c->serialNumber = serial;
    c->Bm = tx->input.Bm;
    c->currentDensityTarget = tx->input.cdav;
    c->windowAspectRatio = tx->input.windowAspectRatio;
    c->calculated = calculated && isfinite(tx->performance.ptFL) &&
        isfinite(tx->tank.Wtot) && isfinite(tx->tankDerived.materialCostIndex) &&
        isfinite(tx->performance.cases[0].efficiency) && isfinite(tx->performance.cases[1].efficiency) && isfinite(tx->performance.Ez) &&
        isfinite(tx->tank.TrWithTubes) && isfinite(tx->tank.KgPkva) &&
        isfinite(tx->noLoadCurrent.I0byI2) && isfinite(tx->tank.Vt);
    if (!c->calculated) {
        strcpy(c->constraintFailures, "Calculation failed or non-finite output");
        return;
    }
    c->feasible = isSafeFeasible(tx);
    c->totalLossW = tx->performance.ptFL * 1000.0;
    c->activeMassKg = tx->tank.Wtot;
    c->materialCostIndex = tx->tankDerived.materialCostIndex;
    c->efficiencyPercent = tx->performance.cases[0].efficiency;
    c->comparisonEfficiencyPercent = tx->performance.cases[1].efficiency;
    c->impedancePercent = tx->performance.Ez * 100.0;
    c->temperatureRiseC = tx->tank.TrWithTubes;
    c->benchmarkPassCount = benchmarkPassCount(tx);
    c->specificMassKgKva = tx->tank.KgPkva;
    c->noLoadCurrentPercent = tx->noLoadCurrent.I0byI2;
    c->tankVolumeM3 = tx->tank.Vt;
#define FAILURE(condition, label) if (!(condition)) strcat(c->constraintFailures, label "; ")
    FAILURE(tx->lv.cd >= 2.3 && tx->lv.cd <= 3.5, "LV current density");
    FAILURE(tx->hv.cd >= 2.3 && tx->hv.cd <= 3.5, "HV current density");
    FAILURE(tx->lv.SlkAx >= 7.0, "LV axial clearance");
    FAILURE(tx->hv.SlkAx >= 7.0, "HV axial clearance");
    FAILURE(tx->hv.endCoilTurns > 0.0, "HV end-coil turns");
    FAILURE(tx->magneticFrame.D * 1000.0 - tx->hv.do_ >= 15.0, "Adjacent winding clearance");
    FAILURE(tx->tank.TrWithTubes <= tx->input.TRP + 0.05, "Cooled temperature rise");
    FAILURE(tx->tankDerived.Voil > 0.0, "Oil volume");
#undef FAILURE
}

static double criterionValue(const OptimizationCandidate *c, int criterion)
{
    if (criterion == 0) return -c->comparisonEfficiencyPercent;
    if (criterion == 1) return c->specificMassKgKva;
    if (criterion == 2) return c->noLoadCurrentPercent;
    return c->tankVolumeM3;
}

int runOptimization(const Transformer *baseline, OptimizationSet *set)
{
    OptimizationCandidate pool[MAX_SEARCH_DESIGNS];
    OptimizationCandidate frontier[MAX_SEARCH_DESIGNS];
    int pareto[MAX_SEARCH_DESIGNS];
    int poolCount = 0;
    memset(set, 0, sizeof(*set));
    set->recommendedIndex = -1;

    const DesignInputs *bounds = &baseline->input;
    for (int bi = 0; bi <= 6; bi++) {
        for (int ji = 0; ji <= 6; ji++) {
            for (int ai = 0; ai <= 5; ai++) {
                Transformer candidate = *baseline;
                candidate.input.Bm = bounds->optimizerBmMin +
                    (bounds->optimizerBmMax - bounds->optimizerBmMin) * bi / 6.0;
                candidate.input.cdav = bounds->optimizerCurrentDensityMin +
                    (bounds->optimizerCurrentDensityMax - bounds->optimizerCurrentDensityMin) * ji / 6.0;
                candidate.input.windowAspectRatio = bounds->optimizerAspectRatioMin +
                    (bounds->optimizerAspectRatioMax - bounds->optimizerAspectRatioMin) * ai / 5.0;
                candidate.input.automaticConductorSizing = true;
                candidate.runMode = RUN_OPTIMIZE;
                set->evaluatedDesigns++;
                int calculated = runSimulation(&candidate, SECTION_ALL) == 0;
                OptimizationCandidate result;
                summarize(&candidate, set->evaluatedDesigns, calculated, &result);
                /* Inclusive, evenly spaced samples of the original attempt sequence. */
                if (set->sampleCount < OPTIMIZATION_SAMPLE_COUNT &&
                    result.serialNumber == 1 + (int)lround((double)set->sampleCount *
                        (SEARCH_DESIGNS - 1) / (OPTIMIZATION_SAMPLE_COUNT - 1)))
                    set->samples[set->sampleCount++] = result;
                if (!result.feasible) continue;
                set->feasibleDesigns++;
                for (int criterion = 0; criterion < OPTIMIZATION_CRITERIA_COUNT; criterion++) {
                    OptimizationCandidate *winner = &set->criteriaWinners[criterion];
                    if (!winner->serialNumber || criterionValue(&result, criterion) < criterionValue(winner, criterion))
                        *winner = result;
                }
                if (poolCount >= MAX_SEARCH_DESIGNS) continue;
                pool[poolCount++] = result;
            }
        }
    }

    if (poolCount == 0) return -1;
    int paretoCount = 0;
    for (int i = 0; i < poolCount; i++) {
        int dominated = 0;
        for (int j = 0; j < poolCount && !dominated; j++) {
            if (i != j && dominates(&pool[j], &pool[i])) dominated = 1;
        }
        if (!dominated) pareto[paretoCount++] = i;
    }

    double minLoss = DBL_MAX, maxLoss = -DBL_MAX;
    double minMass = DBL_MAX, maxMass = -DBL_MAX;
    double minCost = DBL_MAX, maxCost = -DBL_MAX;
    for (int i = 0; i < paretoCount; i++) {
        OptimizationCandidate *c = &pool[pareto[i]];
        if (c->totalLossW < minLoss) minLoss = c->totalLossW;
        if (c->totalLossW > maxLoss) maxLoss = c->totalLossW;
        if (c->activeMassKg < minMass) minMass = c->activeMassKg;
        if (c->activeMassKg > maxMass) maxMass = c->activeMassKg;
        if (c->materialCostIndex < minCost) minCost = c->materialCostIndex;
        if (c->materialCostIndex > maxCost) maxCost = c->materialCostIndex;
    }

    for (int i = 0; i < paretoCount; i++) {
        OptimizationCandidate candidate = pool[pareto[i]];
        double lossRange = maxLoss - minLoss;
        double massRange = maxMass - minMass;
        double costRange = maxCost - minCost;
        double loss = lossRange > 0.0 ? (candidate.totalLossW - minLoss) / lossRange : 0.0;
        double mass = massRange > 0.0 ? (candidate.activeMassKg - minMass) / massRange : 0.0;
        double cost = costRange > 0.0 ? (candidate.materialCostIndex - minCost) / costRange : 0.0;
        candidate.balanceScore = sqrt(loss * loss + mass * mass + cost * cost);
        frontier[i] = candidate;
    }

    set->paretoCount = paretoCount;
    qsort(frontier, (size_t)paretoCount, sizeof(frontier[0]), compareBalance);
    set->count = paretoCount < MAX_OPTIMIZATION_RESULTS ? paretoCount : MAX_OPTIMIZATION_RESULTS;
    memcpy(set->candidates, frontier, (size_t)set->count * sizeof(frontier[0]));
    set->recommendedIndex = set->count > 0 ? 0 : -1;
    return set->count > 0 ? 0 : -1;
}
