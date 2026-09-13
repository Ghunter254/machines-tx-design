/* Synthetic search: 294 non-dominated designs, best balance after the old 32-row cutoff. */
#include "../include/transformer_design.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int attempt;
static int failureMode;
int runSimulation(Transformer *tx, unsigned sections)
{
    (void)sections;
    int serial = ++attempt;
    if (failureMode == 2) return -1;
    if (failureMode == 3) serial = 1;
    tx->lv.cd = tx->hv.cd = failureMode == 1 ? 20.0 : 2.6;
    tx->lv.SlkAx = tx->hv.SlkAx = 20;
    tx->hv.endCoilTurns = 5;
    tx->magneticFrame.D = 1;
    tx->hv.do_ = 400;
    tx->tank.TrWithTubes = 40;
    tx->tankDerived.Voil = 1;
    tx->performance.ptFL = serial;
    tx->tank.Wtot = 295 - serial;
    tx->tankDerived.materialCostIndex = 295 - serial;
    tx->performance.cases[0].efficiency = 99;
    tx->performance.cases[1].efficiency = 90 + serial / 100.0;
    tx->tank.KgPkva = 295 - serial;
    tx->noLoadCurrent.I0byI2 = 295 - serial;
    tx->tank.Vt = 295 - serial;
    return 0;
}

int main(void)
{
    Transformer tx = {0};
    tx.input.TRP = 50;
    OptimizationSet set;
    assert(runOptimization(&tx, &set) == 0);
    assert(set.paretoCount == 294 && set.count == 32);
    /* loss=x, mass=cost=1-x; minimum distance is at x=2/3. */
    assert(set.candidates[0].serialNumber == 196);
    for (int i = 0; i < 4; i++) assert(set.criteriaWinners[i].serialNumber == 294);
    assert(set.samples[0].serialNumber == 1 && set.samples[14].serialNumber == 294);
    for (int mode = 1; mode <= 2; mode++) {
        failureMode = mode; attempt = 0;
        assert(runOptimization(&tx, &set) == -1);
        assert(set.feasibleDesigns == 0 && set.count == 0 && set.recommendedIndex == -1);
        assert(set.sampleCount == 15);
        for (int i = 0; i < 15; i++) {
            assert(!set.samples[i].feasible);
            assert(set.samples[i].calculated == (mode == 1));
            assert(strlen(set.samples[i].constraintFailures) > 0);
        }
        for (int i = 0; i < 4; i++) assert(set.criteriaWinners[i].serialNumber == 0);
    }
    failureMode = 3; attempt = 0;
    assert(runOptimization(&tx, &set) == 0);
    assert(set.paretoCount == 294 && set.candidates[0].serialNumber == 1);
    for (int i = 0; i < 4; i++) assert(set.criteriaWinners[i].serialNumber == 1);
    puts("Passed full-frontier ranking, four global winners, exact ties, rejected and failed searches.");
    return 0;
}
