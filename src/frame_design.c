#include "../include/frame_design.h"

#include <math.h>

static double roundUp(double value, double step)
{
    return step > 0.0 ? ceil((value - 1e-12) / step) * step : value;
}

void designFrame(Transformer *tx)
{
    DesignInputs *in = &tx->input;
    MagneticFrame *frame = &tx->magneticFrame;

    frame->Et = in->K * sqrt(in->KVA / in->Ph);
    frame->Ai = frame->Et / (4.44 * in->f * in->Bm);
    frame->d = roundUp(sqrt(frame->Ai / in->k), in->dimensionRoundingM);
    frame->Ai = in->k * frame->d * frame->d;
    frame->Et = 4.44 * in->f * in->Bm * frame->Ai;

    frame->kw = (10.0 / (30.0 + in->HV / 1000.0)) * in->windowSpaceMultiplier;
    frame->Aw = (in->KVA * 1000.0) /
        (3.33 * in->f * in->Bm * frame->kw * in->cdav * 1e6 * frame->Ai);
    frame->L = roundUp(sqrt(in->windowAspectRatio * frame->Aw), in->dimensionRoundingM);
    frame->D = roundUp(frame->Aw / frame->L + frame->d, in->dimensionRoundingM);
    frame->windowRatio = frame->L / (frame->D - frame->d);

    frame->W = roundUp(2.0 * frame->D + in->yokeWidthFactor * frame->d,
                       in->dimensionRoundingM);
    frame->Ac = frame->Ai / in->ki;
    frame->Ay = in->yokeAreaFactor * frame->Ac;
    frame->by = in->yokeWidthFactor * frame->d;
    frame->hy = frame->Ay / frame->by;
    frame->By = (frame->Ac / frame->Ay) * in->Bm;

    frame->WpKgC = in->coreLossReferenceWKg *
        pow(in->Bm / in->coreLossReferenceFluxT, in->lossCurveExponent);
    frame->WpKgY = in->yokeLossReferenceWKg *
        pow(frame->By / in->yokeLossReferenceFluxT, in->lossCurveExponent);
    frame->KgC = in->Ph * frame->Ac * frame->L * in->density_fe;
    frame->PiC = frame->WpKgC * frame->KgC;
    frame->KgY = 2.0 * frame->Ay * frame->W * in->density_fe;
    frame->PiY = frame->WpKgY * frame->KgY;
    frame->Pi = in->ironLossBuildFactor * (frame->PiC + frame->PiY) / 1000.0;
}
