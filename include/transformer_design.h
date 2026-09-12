#ifndef TRANSFORMER_DESIGN_H
#define TRANSFORMER_DESIGN_H

#include <stdbool.h>
#include <stdio.h>

#define PERFORMANCE_CASES 4
#define MAX_EVALUATIONS 24
#define MAX_OPTIMIZATION_RESULTS 32

typedef enum { CONNECTION_STAR = 0, CONNECTION_DELTA = 1 } ConnectionKind;
typedef enum { RUN_NOMINAL = 0, RUN_EXPLORE = 1, RUN_OPTIMIZE = 2 } RunMode;
typedef enum {
    SECTION_FRAME = 1 << 0,
    SECTION_NO_LOAD = 1 << 1,
    SECTION_LV = 1 << 2,
    SECTION_HV = 1 << 3,
    SECTION_PERFORMANCE = 1 << 4,
    SECTION_TANK = 1 << 5,
    SECTION_ALL = (1 << 6) - 1
} DesignSection;
typedef enum { STATUS_PASS, STATUS_WARN, STATUS_FAIL, STATUS_INFO } EvaluationStatus;

typedef struct
{
    double KVA, HV, LV;
    int Ph;
    double f;
    ConnectionKind hvConnection, lvConnection;
    int vectorClock;
    char cooling[12];

    double TRP, referenceTemperatureC, ambientTemperatureC;
    double density_cu, density_fe, density_oil;
    double copperResistivity20, copperTemperatureConstant;

    double k, K, Bm, cdav, ki, windowAspectRatio;
    double yokeAreaFactor, yokeWidthFactor, windowSpaceMultiplier, dimensionRoundingM;
    double coreLossReferenceFluxT, coreLossReferenceWKg;
    double yokeLossReferenceFluxT, yokeLossReferenceWKg;
    double lossCurveExponent, ironLossBuildFactor;
    double coreATPerMeter, yokeATPerMeter, excitationBuildFactor;

    double conductorInsulationMm, copperStrayLossFactor;

    int lvTurnsRadially, lvParallelStrands, lvAxialStrands;
    double lvStrandWidthMm, lvStrandThicknessMm, lvEdgeFactor, lvWindingHeightFraction;
    double lvInterTurnInsulationMm, lvEndInsulationMm, lvRadialInsulationMm;
    double coreToLvOilDuctMm, coreToLvCylinderMm, lvFormerToWindingDuctMm;

    int hvCoils, hvAxialStrands, hvRadialStrands;
    double hvStrandWidthMm, hvStrandThicknessMm, hvEdgeFactor, hvWindingHeightFraction;
    double hvInterCoilInsulationMm, hvEndRingMm, hvEndInsulationMm;
    double lvToHvOilDuctMm, lvToHvCylinderMm, hvFormerToWindingDuctMm;

    double Dct, Hct, dL, dB, dH;
    double plainTankDissipation, tubeCoefficient, tubeEffectiveness, tankPlateThicknessM;

    double optimizerBmMin, optimizerBmMax;
    double optimizerCurrentDensityMin, optimizerCurrentDensityMax;
    double optimizerAspectRatioMin, optimizerAspectRatioMax;
    double copperCostIndex, coreSteelCostIndex, tankSteelCostIndex, oilCostIndex;
    bool automaticConductorSizing;
} DesignInputs;

typedef struct
{
    double Et, Ai, d, Ac, kw, Aw, L, D, windowRatio, W, Ay, by, hy;
    double WpKgC, KgC, PiC, By, WpKgY, KgY, PiY, Pi;
} MagneticFrame;

typedef struct
{
    double atC, atY, ATC, ATY, ATpPh, T2, I2, Iw, Im, I0, I0byI2;
} NoLoadCurrent;

typedef struct
{
    double phaseVoltage, T, Tr, Ta, middleCoilTurns, endCoilTurns;
    double I, a, cd, ALW, ALT, activeAxialLength, ALWx, SlkAx;
    double stP, NstA, NstR, stW, stT;
    double rw, di, do_, Lmt, Lcu, Vcu, Wcu;
    double r20, rReference, pcu20, pcuReference;
} Winding;

typedef struct
{
    double pf, loadPU, losses, output, input, efficiency;
} PerformanceCase;

typedef struct
{
    double pcuT20, pcuT, ptFL, Ldmxef, efmx, Lmt, Lc, AT;
    double Er, Ex, Ez, Reg85, RegUPF;
    PerformanceCase cases[PERFORMANCE_CASES];
} Performance;

typedef struct
{
    double dL, dB, dH, Lt, bt, ht, Vt, St, Tr, TrWithTubes, TRP;
    double Dct, Hct, At, CAt;
    int Nt;
    double Wcu1, Wcu2, Wiron, Wtot, KgPkva;
} Tank;

typedef struct
{
    double Wsteel, Voil, Woil, Wship, materialCostIndex;
} TankDerived;

typedef struct
{
    char id[40], label[72], unit[20], source[72], meaning[220], verdict[220];
    double value, limitLow, limitHigh;
    EvaluationStatus status;
} Evaluation;

typedef struct
{
    DesignInputs input;
    MagneticFrame magneticFrame;
    NoLoadCurrent noLoadCurrent;
    Winding lv, hv;
    Performance performance;
    Tank tank;
    TankDerived tankDerived;
    Evaluation evaluations[MAX_EVALUATIONS];
    int evaluationCount;
    unsigned requestedSections, completedSections;
    RunMode runMode;
} Transformer;

typedef struct
{
    double Bm, currentDensityTarget, windowAspectRatio;
    double totalLossW, activeMassKg, materialCostIndex;
    double efficiencyPercent, impedancePercent, temperatureRiseC;
    int benchmarkPassCount;
    double balanceScore;
} OptimizationCandidate;

typedef struct
{
    OptimizationCandidate candidates[MAX_OPTIMIZATION_RESULTS];
    int count, feasibleDesigns, evaluatedDesigns, recommendedIndex;
} OptimizationSet;

const char *connectionName(ConnectionKind connection);
const char *connectionShortName(ConnectionKind connection);
int connectionFromString(const char *text, ConnectionKind *connection);
const char *runModeName(RunMode mode);
const char *statusName(EvaluationStatus status);
double windingPhaseVoltage(double lineVoltage, ConnectionKind connection);
double windingPhaseCurrent(double kva, double lineVoltage, int phases, ConnectionKind connection);

int runSimulation(Transformer *tx, unsigned requestedSections);
void evaluateTransformer(Transformer *tx);
int runOptimization(const Transformer *baseline, OptimizationSet *set);
void printTransformerResults(const Transformer *tx);
int writeTextReport(const Transformer *tx, const OptimizationSet *optimization, const char *filename);
int writeJsonReport(const Transformer *tx, const OptimizationSet *optimization, const char *filename);
int writeJsonStream(const Transformer *tx, const OptimizationSet *optimization, FILE *stream);

#endif
