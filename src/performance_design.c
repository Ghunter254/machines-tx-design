#include "../include/performance_design.h"

#include <math.h>

#define PI 3.14159265358979323846

void designPerformance(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    Performance *p = &tx->performance;
    static const double powerFactors[PERFORMANCE_CASES] = {1.0, 0.85, 0.85, 0.85};
    static const double loads[PERFORMANCE_CASES] = {1.0, 1.0, 0.75, 0.5};

    p->pcuT20 = in->copperStrayLossFactor * (tx->lv.pcu20 + tx->hv.pcu20);
    p->pcuT = in->copperStrayLossFactor *
        (tx->lv.pcuReference + tx->hv.pcuReference);
    p->ptFL = p->pcuT + tx->magneticFrame.Pi;

    for (int i = 0; i < PERFORMANCE_CASES; i++) {
        PerformanceCase *c = &p->cases[i];
        c->pf = powerFactors[i];
        c->loadPU = loads[i];
        c->losses = tx->magneticFrame.Pi + p->pcuT * c->loadPU * c->loadPU;
        c->output = c->loadPU * in->KVA * c->pf;
        c->input = c->output + c->losses;
        c->efficiency = c->output / c->input * 100.0;
    }

    p->Ldmxef = sqrt(tx->magneticFrame.Pi / p->pcuT) * in->KVA;
    const double maxOutput = p->Ldmxef * 0.85;
    p->efmx = maxOutput / (maxOutput + 2.0 * tx->magneticFrame.Pi) * 100.0;
    p->Lmt = (tx->lv.Lmt + tx->hv.Lmt) / 2.0;
    p->Lc = tx->hv.activeAxialLength / 1000.0;
    p->AT = tx->hv.I * tx->hv.T;
    p->Ex = 2.0 * PI * in->f * 4.0 * PI * 1e-7 * p->Lmt * p->AT /
        (p->Lc * tx->magneticFrame.Et) *
        (0.016 + (tx->hv.rw + tx->lv.rw) / 3000.0);
    p->Er = p->pcuT / in->KVA;
    p->Ez = sqrt(p->Er * p->Er + p->Ex * p->Ex);
    p->Reg85 = p->Er * 0.85 + p->Ex * sqrt(1.0 - 0.85 * 0.85);
    p->RegUPF = p->Er;
}
