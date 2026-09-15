#include "../include/tank_design.h"

#include <math.h>

#define PI 3.14159265358979323846

void designTank(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    Tank *tank = &tx->tank;
    TankDerived *derived = &tx->tankDerived;

    tank->dL = in->dL; tank->dB = in->dB; tank->dH = in->dH;
    tank->Dct = in->Dct; tank->Hct = in->Hct; tank->TRP = in->TRP;
    tank->Lt = 2.0 * tx->magneticFrame.D + tx->hv.do_ / 1000.0 + in->dL;
    tank->bt = tx->hv.do_ / 1000.0 + in->dB;
    tank->ht = tx->magneticFrame.L + 2.0 * tx->magneticFrame.hy + in->dH;
    tank->Vt = tank->Lt * tank->bt * tank->ht;
    tank->St = 2.0 * (tank->bt + tank->Lt) * tank->ht;

    tank->Tr = tx->performance.ptFL * 1000.0 /
        (in->plainTankDissipation * tank->St);
    tank->At = PI * in->Dct * in->Hct;
    const double heatRemovedByTank = in->plainTankDissipation * tank->St * in->TRP;
    const double remainingHeat = tx->performance.ptFL * 1000.0 - heatRemovedByTank;
    if (remainingHeat > 0.0) {
        tank->CAt = remainingHeat /
            (in->tubeCoefficient * in->TRP * in->tubeEffectiveness);
        tank->Nt = (int)ceil(tank->CAt / tank->At);
    } else {
        tank->CAt = 0.0;
        tank->Nt = 0;
    }
    const double totalDissipation = in->plainTankDissipation * tank->St +
        in->tubeCoefficient * in->tubeEffectiveness * tank->Nt * tank->At;
    tank->TrWithTubes = tx->performance.ptFL * 1000.0 / totalDissipation;

    /* Winding design stores one phase. Tank accounting includes every phase. */
    tank->Wcu1 = in->Ph * tx->hv.Wcu;
    tank->Wcu2 = in->Ph * tx->lv.Wcu;
    tank->Wiron = tx->magneticFrame.KgC + tx->magneticFrame.KgY;
    tank->Wtot = 1.01 * (tank->Wcu1 + tank->Wcu2 + tank->Wiron);
    tank->KgPkva = tank->Wtot / in->KVA;

    const double tankPlateArea = tank->St + 2.0 * tank->Lt * tank->bt;
    derived->Wsteel = in->density_fe * tankPlateArea * in->tankPlateThicknessM;
    const double ironVolume = tank->Wiron / in->density_fe;
    derived->Voil = tank->Vt - ironVolume - in->Ph * (tx->lv.Vcu + tx->hv.Vcu);
    if (derived->Voil < 0.0) derived->Voil = 0.0;
    derived->Woil = in->density_oil * derived->Voil;
    derived->Wship = tank->Wtot + derived->Wsteel + derived->Woil;
    derived->materialCostIndex =
        (tank->Wcu1 + tank->Wcu2) * in->copperCostIndex +
        tank->Wiron * in->coreSteelCostIndex +
        derived->Wsteel * in->tankSteelCostIndex +
        derived->Woil * in->oilCostIndex;
}
