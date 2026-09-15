#include "../include/transformer_design.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double gridValue(double min, double max, int index, int count)
{
    return count == 1 ? min : min + (max - min) * index / (count - 1);
}

static int isSafeFeasible(const Transformer *tx)
{
    const DesignInputs *in = &tx->input;
    const double clearance = tx->magneticFrame.D * 1000.0 - tx->hv.do_;
    return isfinite(tx->performance.ptFL) && isfinite(tx->tank.Wtot) &&
        tx->lv.cd >= in->optimizerActualCurrentDensityMin && tx->lv.cd <= in->optimizerActualCurrentDensityMax &&
        tx->hv.cd >= in->optimizerActualCurrentDensityMin && tx->hv.cd <= in->optimizerActualCurrentDensityMax &&
        tx->lv.SlkAx >= in->optimizerMinAxialSlackMm && tx->hv.SlkAx >= in->optimizerMinAxialSlackMm &&
        tx->hv.endCoilTurns > 0.0 && clearance >= in->optimizerMinAdjacentClearanceMm &&
        tx->tank.TrWithTubes <= in->TRP + in->optimizerTemperatureMarginC &&
        tx->tankDerived.Voil > 0.0 &&
        tx->performance.cases[1].efficiency >= in->optimizerMinEfficiencyPercent &&
        tx->tank.KgPkva <= in->optimizerMaxSpecificMassKgKva &&
        tx->noLoadCurrent.I0byI2 <= in->optimizerMaxNoLoadCurrentPercent &&
        tx->tank.Vt <= in->optimizerMaxTankVolumeM3;
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
    c->K = tx->input.K;
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
    c->d = tx->magneticFrame.d; c->L = tx->magneticFrame.L;
    c->D = tx->magneticFrame.D; c->W = tx->magneticFrame.W;
    c->actualWindowRatio = tx->magneticFrame.windowRatio;
    c->lvCurrentDensity = tx->lv.cd; c->hvCurrentDensity = tx->hv.cd;
    c->regulationPercent = tx->performance.Reg85 * 100.0;
    c->coolingTubes = (int)tx->tank.Nt;
#define FAILURE(condition, label) if (!(condition)) strcat(c->constraintFailures, label "; ")
    FAILURE(tx->lv.cd >= tx->input.optimizerActualCurrentDensityMin && tx->lv.cd <= tx->input.optimizerActualCurrentDensityMax, "LV current density");
    FAILURE(tx->hv.cd >= tx->input.optimizerActualCurrentDensityMin && tx->hv.cd <= tx->input.optimizerActualCurrentDensityMax, "HV current density");
    FAILURE(tx->lv.SlkAx >= tx->input.optimizerMinAxialSlackMm, "LV axial fit");
    FAILURE(tx->hv.SlkAx >= tx->input.optimizerMinAxialSlackMm, "HV axial fit");
    FAILURE(tx->hv.endCoilTurns > 0.0, "HV end-coil turns");
    FAILURE(tx->magneticFrame.D * 1000.0 - tx->hv.do_ >= tx->input.optimizerMinAdjacentClearanceMm, "Winding overlap");
    FAILURE(tx->tank.TrWithTubes <= tx->input.TRP + tx->input.optimizerTemperatureMarginC, "Cooled temperature rise");
    FAILURE(tx->tankDerived.Voil > 0.0, "Oil volume");
    FAILURE(tx->performance.cases[1].efficiency >= tx->input.optimizerMinEfficiencyPercent, "Minimum efficiency");
    FAILURE(tx->tank.KgPkva <= tx->input.optimizerMaxSpecificMassKgKva, "Maximum kg/kVA");
    FAILURE(tx->noLoadCurrent.I0byI2 <= tx->input.optimizerMaxNoLoadCurrentPercent, "Maximum I0/I2");
    FAILURE(tx->tank.Vt <= tx->input.optimizerMaxTankVolumeM3, "Maximum tank volume");
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
    int poolCount = 0;
    memset(set, 0, sizeof(*set));
    set->recommendedIndex = -1;

    const DesignInputs *bounds = &baseline->input;
    const double requested = (double)bounds->optimizerKSteps * bounds->optimizerBmSteps *
        bounds->optimizerCurrentDensitySteps * bounds->optimizerAspectRatioSteps;
    if (bounds->optimizerKSteps < 1 || bounds->optimizerBmSteps < 1 ||
        bounds->optimizerCurrentDensitySteps < 1 || bounds->optimizerAspectRatioSteps < 1 ||
        requested > MAX_SEARCH_DESIGNS) return -2;
    const int total = (int)requested;
    const int sampleTarget = total < OPTIMIZATION_SAMPLE_COUNT ? total : OPTIMIZATION_SAMPLE_COUNT;
    OptimizationCandidate *pool = calloc((size_t)total, sizeof(*pool));
    OptimizationCandidate *frontier = calloc((size_t)total, sizeof(*frontier));
    int *pareto = calloc((size_t)total, sizeof(*pareto));
    if (!pool || !frontier || !pareto) { free(pool); free(frontier); free(pareto); return -2; }
    for (int ki = 0; ki < bounds->optimizerKSteps; ki++) {
      for (int bi = 0; bi < bounds->optimizerBmSteps; bi++) {
        for (int ji = 0; ji < bounds->optimizerCurrentDensitySteps; ji++) {
            for (int ai = 0; ai < bounds->optimizerAspectRatioSteps; ai++) {
                Transformer candidate = *baseline;
                candidate.input.K = gridValue(bounds->optimizerKMin, bounds->optimizerKMax, ki, bounds->optimizerKSteps);
                candidate.input.Bm = gridValue(bounds->optimizerBmMin, bounds->optimizerBmMax, bi, bounds->optimizerBmSteps);
                candidate.input.cdav = gridValue(bounds->optimizerCurrentDensityMin, bounds->optimizerCurrentDensityMax, ji, bounds->optimizerCurrentDensitySteps);
                candidate.input.windowAspectRatio = gridValue(bounds->optimizerAspectRatioMin, bounds->optimizerAspectRatioMax, ai, bounds->optimizerAspectRatioSteps);
                candidate.input.automaticConductorSizing = true;
                candidate.runMode = RUN_OPTIMIZE;
                set->evaluatedDesigns++;
                int calculated = runSimulation(&candidate, SECTION_ALL) == 0;
                OptimizationCandidate result;
                summarize(&candidate, set->evaluatedDesigns, calculated, &result);
                if (result.calculated) {
                    set->calculatedDesigns++;
                    for (int criterion = 0; criterion < OPTIMIZATION_CRITERIA_COUNT; criterion++) {
                        OptimizationCandidate *winner = &set->calculatedCriteriaWinners[criterion];
                        if (!winner->serialNumber || criterionValue(&result, criterion) < criterionValue(winner, criterion))
                            *winner = result;
                    }
                }
                /* Inclusive, evenly spaced samples of the original attempt sequence. */
                if (set->sampleCount < sampleTarget &&
                    result.serialNumber == 1 + (sampleTarget == 1 ? 0 : (int)lround((double)set->sampleCount *
                        (total - 1) / (sampleTarget - 1))))
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
    }

    if (poolCount == 0) { free(pool); free(frontier); free(pareto); return -1; }
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
    free(pool); free(frontier); free(pareto);
    return set->count > 0 ? 0 : -1;
}
