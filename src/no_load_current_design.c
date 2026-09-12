#include "../include/no_load_current_design.h"

#include <math.h>

void designNoLoadCurrent(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    MagneticFrame *frame = &tx->magneticFrame;
    NoLoadCurrent *nl = &tx->noLoadCurrent;
    const double lvPhaseVoltage = windingPhaseVoltage(in->LV, in->lvConnection);

    nl->atC = in->coreATPerMeter;
    nl->atY = in->yokeATPerMeter;
    nl->ATC = in->Ph * nl->atC * frame->L;
    nl->ATY = 2.0 * nl->atY * frame->W;
    nl->ATpPh = (nl->ATC + nl->ATY) / in->Ph;

    nl->T2 = ceil(lvPhaseVoltage / frame->Et);
    nl->I2 = windingPhaseCurrent(in->KVA, in->LV, in->Ph, in->lvConnection);
    nl->Iw = frame->Pi * 1000.0 / (in->Ph * lvPhaseVoltage);
    nl->Im = in->excitationBuildFactor * nl->ATpPh / (sqrt(2.0) * nl->T2);
    nl->I0 = sqrt(nl->Iw * nl->Iw + nl->Im * nl->Im);
    nl->I0byI2 = nl->I0 / nl->I2 * 100.0;
}
