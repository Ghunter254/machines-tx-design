#include "../include/lv_windings_design.h"

#include <math.h>

#define PI 3.14159265358979323846

static double roundUpTenth(double value)
{
    return ceil((value - 1e-12) * 10.0) / 10.0;
}

void designLVWindings(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    Winding *lv = &tx->lv;

    lv->phaseVoltage = windingPhaseVoltage(in->LV, in->lvConnection);
    lv->T = tx->noLoadCurrent.T2;
    lv->I = tx->noLoadCurrent.I2;
    lv->Tr = in->lvTurnsRadially;
    lv->Ta = ceil(lv->T / lv->Tr);
    lv->stP = in->lvParallelStrands;
    lv->NstA = in->lvAxialStrands;
    lv->NstR = lv->stP / lv->NstA;
    lv->stW = in->lvStrandWidthMm;
    lv->stT = in->lvStrandThicknessMm;

    if (in->automaticConductorSizing) {
        const double required = lv->I /
            (in->cdav * lv->stW * lv->stP * in->lvEdgeFactor);
        lv->stT = roundUpTenth(required);
    }

    lv->ALW = in->lvWindingHeightFraction * tx->magneticFrame.L * 1000.0;
    lv->ALT = lv->ALW / lv->Ta;
    lv->activeAxialLength =
        ((lv->stW + in->conductorInsulationMm) * lv->NstA +
         in->lvInterTurnInsulationMm) * lv->Ta;
    lv->ALWx = lv->activeAxialLength + in->lvEndInsulationMm;
    lv->SlkAx = tx->magneticFrame.L * 1000.0 - lv->ALWx;

    lv->a = lv->stW * lv->stT * lv->stP * in->lvEdgeFactor;
    lv->cd = lv->I / lv->a;
    lv->rw = lv->NstR * (lv->stT + in->conductorInsulationMm) * lv->Tr +
             in->lvRadialInsulationMm;
    lv->di = tx->magneticFrame.d * 1000.0 + 2.0 *
        (in->coreToLvOilDuctMm + in->coreToLvCylinderMm + in->lvFormerToWindingDuctMm);
    lv->do_ = lv->di + 2.0 * lv->rw;
    lv->Lmt = PI * (lv->di + lv->do_) / 2000.0;

    lv->Lcu = lv->Lmt * lv->T;
    lv->Vcu = lv->Lcu * lv->a * 1e-6;
    lv->Wcu = lv->Vcu * in->density_cu;
    lv->r20 = in->copperResistivity20 * lv->Lcu / lv->a;
    lv->rReference = lv->r20 *
        (in->copperTemperatureConstant + in->referenceTemperatureC) /
        (in->copperTemperatureConstant + 20.0);
    lv->pcu20 = in->Ph * lv->I * lv->I * lv->r20 / 1000.0;
    lv->pcuReference = in->Ph * lv->I * lv->I * lv->rReference / 1000.0;
}
