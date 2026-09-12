#include "../include/transformer_design.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SEARCH_DESIGNS 512

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
    return 0;
}

int runOptimization(const Transformer *baseline, OptimizationSet *set)
{
    OptimizationCandidate pool[MAX_SEARCH_DESIGNS];
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
                if (runSimulation(&candidate, SECTION_ALL) != 0 || !isSafeFeasible(&candidate)) continue;
                set->feasibleDesigns++;
                if (poolCount >= MAX_SEARCH_DESIGNS) continue;

                OptimizationCandidate *result = &pool[poolCount++];
                result->Bm = candidate.input.Bm;
                result->currentDensityTarget = candidate.input.cdav;
                result->windowAspectRatio = candidate.input.windowAspectRatio;
                result->totalLossW = candidate.performance.ptFL * 1000.0;
                result->activeMassKg = candidate.tank.Wtot;
                result->materialCostIndex = candidate.tankDerived.materialCostIndex;
                result->efficiencyPercent = candidate.performance.cases[0].efficiency;
                result->impedancePercent = candidate.performance.Ez * 100.0;
                result->temperatureRiseC = candidate.tank.TrWithTubes;
                result->benchmarkPassCount = benchmarkPassCount(&candidate);
                result->balanceScore = 0.0;
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
        if (set->count < MAX_OPTIMIZATION_RESULTS) set->candidates[set->count++] = candidate;
    }

    qsort(set->candidates, (size_t)set->count, sizeof(set->candidates[0]), compareBalance);
    set->recommendedIndex = set->count > 0 ? 0 : -1;
    return set->count > 0 ? 0 : -1;
}
