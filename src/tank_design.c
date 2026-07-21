#include "../include/transformer_design.h"
#include <math.h>
#include <stdio.h>

#define PI 3.14159265358979323846

static void calculateTankDimensions(Transformer *tx);
static void calculateTankVolume(Transformer *tx);
static void calculateCoolingSurface(Transformer *tx);
static void calculateThermalPerformance(Transformer *tx);
static void calculateWeights(Transformer *tx);

void designTank(Transformer *tx)
{
    calculateTankDimensions(tx);
    calculateTankVolume(tx);
    calculateCoolingSurface(tx);
    calculateWeights(tx);
    calculateThermalPerformance(tx);

    printf("Tank design completed successfully.\n");
}

static void calculateTankDimensions(Transformer *tx)
{
    tx->tank.Lt = (2.0 * tx->magneticFrame.D) + tx->hv.do_/1000 + tx->input.dL;
    tx->tank.bt = tx->hv.do_/1000 + tx->input.dB;
    tx->tank.ht = tx->magneticFrame.L + (2.0 * tx->magneticFrame.hy) + tx->input.dH;
}

static void calculateTankVolume(Transformer *tx)
{
    tx->tank.Vt = tx->tank.Lt * tx->tank.bt * tx->tank.ht;
    double ironWeight = tx->magneticFrame.KgC + tx->magneticFrame.KgY;
    double ironVolume = ironWeight / tx->input.density_fe;
    tx->tankDerived.Voil = tx->tank.Vt - (ironVolume + tx->lv.Vcu + tx->hv.Vcu);
}

static void calculateCoolingSurface(Transformer *tx)
{
    tx->tank.St = 2.0 * (tx->tank.bt + tx->tank.Lt) * tx->tank.ht;
}

static void calculateThermalPerformance(Transformer *tx)
{
    tx->tank.Tr = (tx->performance.ptFL * 1000.0) / (PLAIN_TANK_DISSIPATION * tx->tank.St);
    tx->tank.At = PI * tx->input.Dct * tx->input.Hct;
    double heatRemovedByTank = PLAIN_TANK_DISSIPATION * tx->tank.St * tx->input.TRP;
    double remainingHeat = (tx->performance.ptFL * 1000.0) - heatRemovedByTank;

    if (remainingHeat <= 0.0)
    {
        tx->tank.CAt = 0.0;
        tx->tank.Nt = 0;
        return;
    }

    tx->tank.CAt = remainingHeat / (TUBE_COEFFICIENT * tx->input.TRP * TUBE_EFFECTIVENESS);
    tx->tank.Nt = (int)ceil(tx->tank.CAt / tx->tank.At);
}

static void calculateWeights(Transformer *tx)
{

    tx->tank.Wcu1  = tx->hv.Wcu;
    tx->tank.Wcu2  = tx->lv.Wcu;
    tx->tank.Wiron = tx->magneticFrame.KgC + tx->magneticFrame.KgY;
    tx->tank.Wtot = 1.01 * (tx->tank.Wcu1 + tx->tank.Wcu2 + tx->tank.Wiron);

    tx->tank.KgPkva = tx->tank.Wtot / tx->input.KVA;


    tx->tankDerived.Wsteel = tx->input.density_fe * tx->tank.St * 0.008;
    tx->tankDerived.Woil = tx->input.density_oil * tx->tankDerived.Voil;

    tx->tankDerived.Wship = tx->tank.Wtot + tx->tankDerived.Wsteel + tx->tankDerived.Woil;
}