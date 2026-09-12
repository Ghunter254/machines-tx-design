#include "../include/hv_windings_design.h"

#include <math.h>

#define PI 3.14159265358979323846

static double roundUpTenth(double value)
{
    return ceil((value - 1e-12) * 10.0) / 10.0;
}

void designHVWindings(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    Winding *hv = &tx->hv;
    const double hvPhaseVoltage = windingPhaseVoltage(in->HV, in->hvConnection);
    const double lvPhaseVoltage = windingPhaseVoltage(in->LV, in->lvConnection);

    hv->phaseVoltage = hvPhaseVoltage;
    hv->T = ceil(tx->lv.T * hvPhaseVoltage / lvPhaseVoltage);
    hv->I = windingPhaseCurrent(in->KVA, in->HV, in->Ph, in->hvConnection);
    hv->Ta = in->hvCoils;
    hv->NstA = in->hvAxialStrands;
    hv->NstR = in->hvRadialStrands;
    hv->Tr = hv->NstR;
    hv->stP = 1.0;
    hv->stW = in->hvStrandWidthMm;
    hv->stT = in->hvStrandThicknessMm;

    if (in->automaticConductorSizing) {
        const double required = hv->I / (in->cdav * hv->stW * in->hvEdgeFactor);
        hv->stT = roundUpTenth(required);
    }

    hv->middleCoilTurns = hv->NstA * hv->NstR;
    hv->endCoilTurns = (hv->T - hv->middleCoilTurns * (hv->Ta - 2.0)) / 2.0;
    hv->ALW = in->hvWindingHeightFraction * tx->magneticFrame.L * 1000.0;
    hv->ALT = hv->ALW / hv->Ta;
    hv->activeAxialLength = hv->Ta * hv->NstA *
        (hv->stW + in->conductorInsulationMm) +
        (hv->Ta - 1.0) * in->hvInterCoilInsulationMm;
    hv->ALWx = hv->activeAxialLength + in->hvEndRingMm + in->hvEndInsulationMm;
    hv->SlkAx = tx->magneticFrame.L * 1000.0 - hv->ALWx;

    hv->a = hv->stW * hv->stT * in->hvEdgeFactor;
    hv->cd = hv->I / hv->a;
    hv->rw = hv->NstR * (hv->stT + in->conductorInsulationMm);
    hv->di = tx->lv.do_ + 2.0 *
        (in->lvToHvOilDuctMm + in->lvToHvCylinderMm + in->hvFormerToWindingDuctMm);
    hv->do_ = hv->di + 2.0 * hv->rw;
    hv->Lmt = PI * (hv->di + hv->do_) / 2000.0;

    hv->Lcu = hv->Lmt * hv->T;
    hv->Vcu = hv->Lcu * hv->a * 1e-6;
    hv->Wcu = hv->Vcu * in->density_cu;
    hv->r20 = in->copperResistivity20 * hv->Lcu / hv->a;
    hv->rReference = hv->r20 *
        (in->copperTemperatureConstant + in->referenceTemperatureC) /
        (in->copperTemperatureConstant + 20.0);
    hv->pcu20 = in->Ph * hv->I * hv->I * hv->r20 / 1000.0;
    hv->pcuReference = in->Ph * hv->I * hv->I * hv->rReference / 1000.0;
}
