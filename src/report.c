#include "../include/transformer_design.h"

#include <stdio.h>
#include <string.h>

static void printSectionList(FILE *stream, unsigned sections)
{
    static const char *names[] = {"frame", "no-load", "lv", "hv", "performance", "tank"};
    int first = 1;
    for (int i = 0; i < 6; i++) {
        if (!(sections & (1u << i))) continue;
        fprintf(stream, "%s%s", first ? "" : ", ", names[i]);
        first = 0;
    }
}

static void connectionCode(const Transformer *tx, char *buffer, size_t size)
{
    snprintf(buffer, size, "%s%s%d",
        connectionShortName(tx->input.hvConnection),
        tx->input.lvConnection == CONNECTION_DELTA ? "d" : "y",
        tx->input.vectorClock);
}

static void printEvaluationRange(FILE *out, const Evaluation *evaluation)
{
    if (evaluation->limitHigh >= 1.0e8) {
        fprintf(out, ">= %.3f", evaluation->limitLow);
    } else if (evaluation->limitLow <= 0.0 && evaluation->limitHigh > 0.0) {
        fprintf(out, "<= %.3f", evaluation->limitHigh);
    } else {
        fprintf(out, "%.3f to %.3f", evaluation->limitLow, evaluation->limitHigh);
    }
}

static void writeTextStream(const Transformer *tx, const OptimizationSet *optimization, FILE *out)
{
    char code[16];
    connectionCode(tx, code, sizeof(code));
    fprintf(out, "TRANSFORMER DESIGN SIMULATION REPORT\n");
    fprintf(out, "====================================\n\n");
    fprintf(out, "Run mode          : %s\n", runModeName(tx->runMode));
    fprintf(out, "Requested sections: "); printSectionList(out, tx->requestedSections); fprintf(out, "\n");
    fprintf(out, "Calculated chain  : "); printSectionList(out, tx->completedSections); fprintf(out, "\n");
    fprintf(out, "Rating            : %.1f kVA, %.0f / %.0f V, %.0f Hz, %d phase\n",
        tx->input.KVA, tx->input.HV, tx->input.LV, tx->input.f, tx->input.Ph);
    fprintf(out, "Connection        : %s-%s (%s)\n", connectionName(tx->input.hvConnection),
        connectionName(tx->input.lvConnection), code);
    fprintf(out, "Cooling           : %s\n", tx->input.cooling);
    fprintf(out, "Reference temp.   : %.1f C for winding-loss comparison\n\n",
        tx->input.referenceTemperatureC);

    if (tx->completedSections & SECTION_FRAME) {
        const MagneticFrame *f = &tx->magneticFrame;
        fprintf(out, "1. MAGNETIC FRAME\n-----------------\n");
        fprintf(out, "Volts per turn       Et = K*sqrt(kVA/ph)                   %10.4f V/turn\n", f->Et);
        fprintf(out, "Net core area        Ai = Et/(4.44*f*Bm)                  %10.6f m2\n", f->Ai);
        fprintf(out, "Gross core area      Ac = Ai/ki                           %10.6f m2\n", f->Ac);
        fprintf(out, "Core diameter        d  = sqrt(Ai/k)                      %10.4f m\n", f->d);
        fprintf(out, "Window space factor  kw = 10/(30+HV[kV])*multiplier       %10.4f\n", f->kw);
        fprintf(out, "Window area          Aw                                   %10.6f m2\n", f->Aw);
        fprintf(out, "Core/window height   L                                    %10.4f m\n", f->L);
        fprintf(out, "Core-centre distance D                                    %10.4f m\n", f->D);
        fprintf(out, "Window proportion    L/(D-d)                              %10.4f\n", f->windowRatio);
        fprintf(out, "Yoke length          W = 2D + yoke-width-factor*d         %10.4f m\n", f->W);
        fprintf(out, "Yoke gross area      Ay                                   %10.6f m2\n", f->Ay);
        fprintf(out, "Yoke width / height  by / hy                          %8.4f / %.4f m\n", f->by, f->hy);
        fprintf(out, "Core flux density    Bm                                   %10.4f T\n", tx->input.Bm);
        fprintf(out, "Yoke flux density    By                                   %10.4f T\n", f->By);
        fprintf(out, "Specific core/yoke loss                            %8.4f / %.4f W/kg\n", f->WpKgC, f->WpKgY);
        fprintf(out, "Core / yoke mass                                    %8.2f / %.2f kg\n", f->KgC, f->KgY);
        fprintf(out, "Core / yoke iron loss                              %8.2f / %.2f W\n", f->PiC, f->PiY);
        fprintf(out, "Total iron mass                                          %10.2f kg\n", f->KgC + f->KgY);
        fprintf(out, "Total iron volume                                        %10.6f m3\n", (f->KgC + f->KgY) / tx->input.density_fe);
        fprintf(out, "Iron loss            Pi                                   %10.3f kW\n\n", f->Pi);
    }

    if (tx->completedSections & SECTION_NO_LOAD) {
        const NoLoadCurrent *n = &tx->noLoadCurrent;
        fprintf(out, "2. NO-LOAD CURRENT\n------------------\n");
        fprintf(out, "LV phase voltage                                          %10.3f V\n", tx->lv.phaseVoltage > 0 ? tx->lv.phaseVoltage : windingPhaseVoltage(tx->input.LV, tx->input.lvConnection));
        fprintf(out, "LV turns             T2 = ceil(Vphase/Et)                 %10.0f turns\n", n->T2);
        fprintf(out, "LV phase current                                           %10.3f A\n", n->I2);
        fprintf(out, "Core/yoke AT per m   atC / atY                         %8.2f / %.2f AT/m\n", n->atC, n->atY);
        fprintf(out, "Core/yoke AT         ATC / ATY                         %8.2f / %.2f AT\n", n->ATC, n->ATY);
        fprintf(out, "Total AT per phase   (ATC+ATY)/ph                        %10.3f AT\n", n->ATpPh);
        fprintf(out, "Wattful component    Iw                                   %10.4f A\n", n->Iw);
        fprintf(out, "Magnetizing component Im                                  %10.4f A\n", n->Im);
        fprintf(out, "No-load current      I0 = sqrt(Iw2+Im2)                   %10.4f A\n", n->I0);
        fprintf(out, "No-load ratio        I0/I2                                %10.4f %%\n\n", n->I0byI2);
    }

    if (tx->completedSections & SECTION_LV) {
        const Winding *w = &tx->lv;
        fprintf(out, "3. LOW-VOLTAGE WINDING\n----------------------\n");
        fprintf(out, "Turns / phase current                                  %7.0f / %.3f A\n", w->T, w->I);
        fprintf(out, "LV phase voltage                                          %10.3f V\n", w->phaseVoltage);
        fprintf(out, "Available winding height                                  %10.2f mm\n", w->ALW);
        fprintf(out, "Turns radial / axial                                  %8.0f / %.0f\n", w->Tr, w->Ta);
        fprintf(out, "Conductor arrangement                       %.0f parallel, %.0f x %.0f strands\n", w->stP, w->NstR, w->NstA);
        fprintf(out, "Space per turn       ALT                                  %10.2f mm\n", w->ALT);
        fprintf(out, "Bare strand size                                      %.2f x %.2f mm\n", w->stW, w->stT);
        fprintf(out, "Conductor area / density                         %.3f mm2 / %.3f A/mm2\n", w->a, w->cd);
        fprintf(out, "Active / total axial height                        %.2f / %.2f mm\n", w->activeAxialLength, w->ALWx);
        fprintf(out, "Axial slack                                             %10.2f mm\n", w->SlkAx);
        fprintf(out, "Radial width                                            %10.2f mm\n", w->rw);
        fprintf(out, "Inner / outer diameter                            %.2f mm / %.2f mm\n", w->di, w->do_);
        fprintf(out, "Mean turn / conductor length                       %.6f / %.6f m\n", w->Lmt, w->Lcu);
        fprintf(out, "Copper volume / mass                              %.8f m3 / %.3f kg\n", w->Vcu, w->Wcu);
        fprintf(out, "Resistance at 20 C / %.0f C                       %.6f / %.6f ohm\n", tx->input.referenceTemperatureC, w->r20, w->rReference);
        fprintf(out, "Copper loss at 20 C / %.0f C                        %.4f / %.4f kW\n\n", tx->input.referenceTemperatureC, w->pcu20, w->pcuReference);
    }

    if (tx->completedSections & SECTION_HV) {
        const Winding *w = &tx->hv;
        fprintf(out, "4. HIGH-VOLTAGE WINDING\n-----------------------\n");
        fprintf(out, "Turns / phase current                                  %7.0f / %.3f A\n", w->T, w->I);
        fprintf(out, "HV phase voltage                                          %10.3f V\n", w->phaseVoltage);
        fprintf(out, "Available winding height                                  %10.2f mm\n", w->ALW);
        fprintf(out, "Coils / axial x radial strands                  %.0f / %.0f x %.0f\n", w->Ta, w->NstA, w->NstR);
        fprintf(out, "Coil layout                              %.0f middle turns, %.1f end turns\n", w->middleCoilTurns, w->endCoilTurns);
        fprintf(out, "Space per coil      ALT                                   %10.2f mm\n", w->ALT);
        fprintf(out, "Bare conductor / area                        %.2f x %.2f mm / %.3f mm2\n", w->stW, w->stT, w->a);
        fprintf(out, "Current density                                         %10.3f A/mm2\n", w->cd);
        fprintf(out, "Active / total axial height                        %.2f / %.2f mm\n", w->activeAxialLength, w->ALWx);
        fprintf(out, "Axial slack                                             %10.2f mm\n", w->SlkAx);
        fprintf(out, "Radial width                                            %10.2f mm\n", w->rw);
        fprintf(out, "Inner / outer diameter                            %.2f mm / %.2f mm\n", w->di, w->do_);
        fprintf(out, "Adjacent winding clearance                              %10.2f mm\n", tx->magneticFrame.D * 1000.0 - w->do_);
        fprintf(out, "Mean turn / conductor length                       %.6f / %.6f m\n", w->Lmt, w->Lcu);
        fprintf(out, "Copper volume / mass                              %.8f m3 / %.3f kg\n", w->Vcu, w->Wcu);
        fprintf(out, "Resistance at 20 C / %.0f C                       %.6f / %.6f ohm\n", tx->input.referenceTemperatureC, w->r20, w->rReference);
        fprintf(out, "Copper loss at 20 C / %.0f C                        %.4f / %.4f kW\n\n", tx->input.referenceTemperatureC, w->pcu20, w->pcuReference);
    }

    if (tx->completedSections & SECTION_PERFORMANCE) {
        const Performance *p = &tx->performance;
        fprintf(out, "5. PERFORMANCE\n--------------\n");
        fprintf(out, "LV / HV copper loss at 20 C                      %.3f / %.3f kW\n", tx->lv.pcu20, tx->hv.pcu20);
        fprintf(out, "LV / HV copper loss at %.0f C                    %.3f / %.3f kW\n", tx->input.referenceTemperatureC, tx->lv.pcuReference, tx->hv.pcuReference);
        fprintf(out, "Total copper loss at 20 C / %.0f C                %.3f / %.3f kW\n", tx->input.referenceTemperatureC, p->pcuT20, p->pcuT);
        fprintf(out, "Iron / load / total loss                      %.3f / %.3f / %.3f kW\n", tx->magneticFrame.Pi, p->pcuT, p->ptFL);
        fprintf(out, "Full-load efficiency at unity PF                         %10.3f %%\n", p->cases[0].efficiency);
        fprintf(out, "Maximum efficiency / load                          %.3f %% / %.2f kVA\n", p->efmx, p->Ldmxef);
        fprintf(out, "Mean turn / active coil length                       %.5f / %.5f m\n", p->Lmt, p->Lc);
        fprintf(out, "HV ampere-turns per phase                               %10.2f AT\n", p->AT);
        fprintf(out, "Resistance / reactance / impedance             %.3f / %.3f / %.3f %%\n", p->Er * 100.0, p->Ex * 100.0, p->Ez * 100.0);
        fprintf(out, "Regulation at 0.85 PF / unity PF                   %.3f / %.3f %%\n\n", p->Reg85 * 100.0, p->RegUPF * 100.0);
        fprintf(out, "  PF      load pu      loss kW      output kW      efficiency\n");
        for (int i = 0; i < PERFORMANCE_CASES; i++) {
            const PerformanceCase *c = &p->cases[i];
            fprintf(out, "  %.2f       %.2f        %8.3f       %9.2f        %8.3f %%\n",
                c->pf, c->loadPU, c->losses, c->output, c->efficiency);
        }
        fprintf(out, "\n");
    }

    if (tx->completedSections & SECTION_TANK) {
        const Tank *t = &tx->tank;
        fprintf(out, "6. TANK, COOLING AND MASS\n-------------------------\n");
        fprintf(out, "Tank dimensions L x W x H                     %.3f x %.3f x %.3f m\n", t->Lt, t->bt, t->ht);
        fprintf(out, "Configured clearances L x W x H              %.3f x %.3f x %.3f m\n", t->dL, t->dB, t->dH);
        fprintf(out, "Tank volume / plain surface                       %.3f m3 / %.3f m2\n", t->Vt, t->St);
        fprintf(out, "Plain tank rise / cooled rise                     %.2f C / %.2f C\n", t->Tr, t->TrWithTubes);
        fprintf(out, "Tube diameter / height                            %.3f / %.3f m\n", t->Dct, t->Hct);
        fprintf(out, "Area per tube / required area                     %.3f / %.3f m2\n", t->At, t->CAt);
        fprintf(out, "Cooling tube count                                         %10d\n", t->Nt);
        fprintf(out, "Active / shipping mass                            %.2f / %.2f kg\n", t->Wtot, tx->tankDerived.Wship);
        fprintf(out, "Oil volume / oil mass                             %.3f m3 / %.2f kg\n", tx->tankDerived.Voil, tx->tankDerived.Woil);
        fprintf(out, "Specific active mass                                  %.3f kg/kVA\n\n", t->KgPkva);
        fprintf(out, "BILL OF MATERIALS\n-----------------\n");
        fprintf(out, "HV copper                                             %10.2f kg\n", t->Wcu1);
        fprintf(out, "LV copper                                             %10.2f kg\n", t->Wcu2);
        fprintf(out, "Active core steel                                     %10.2f kg\n", t->Wiron);
        fprintf(out, "Fabricated tank steel                                 %10.2f kg\n", tx->tankDerived.Wsteel);
        fprintf(out, "Insulating oil                                        %10.2f kg\n", tx->tankDerived.Woil);
        fprintf(out, "Material cost index                                  %10.2f\n\n", tx->tankDerived.materialCostIndex);
    }

    fprintf(out, "ENGINEERING ASSESSMENT\n----------------------\n");
    fprintf(out, "Status  Metric                              Result         Expected / benchmark\n");
    for (int i = 0; i < tx->evaluationCount; i++) {
        const Evaluation *e = &tx->evaluations[i];
        fprintf(out, "%-6s  %-34s %9.3f %-7s  ",
            statusName(e->status), e->label, e->value, e->unit);
        printEvaluationRange(out, e);
        fprintf(out, "\n");
        fprintf(out, "        Meaning: %s\n        Verdict: %s\n        Basis:   %s\n",
            e->meaning, e->verdict, e->source);
    }

    if (optimization && optimization->count > 0) {
        fprintf(out, "\nPARETO OPTIMIZATION\n-------------------\n");
        fprintf(out, "Evaluated %d designs; %d passed the hard geometry and thermal constraints.\n",
            optimization->evaluatedDesigns, optimization->feasibleDesigns);
        fprintf(out, "The first row has the smallest equal-weight normalized loss/mass/cost distance across all %d Pareto variants.\n\n", optimization->paretoCount);
        fprintf(out, "  Bm(T)   J(A/mm2)  window ratio  loss(W)  active kg  cost index  eff(%%)  Z(%%)\n");
        for (int i = 0; i < optimization->count; i++) {
            const OptimizationCandidate *c = &optimization->candidates[i];
            fprintf(out, "%c %.3f     %.3f       %.3f       %7.1f   %8.1f   %10.1f  %.3f  %.3f\n",
                i == optimization->recommendedIndex ? '*' : ' ', c->Bm, c->currentDensityTarget,
                c->windowAspectRatio, c->totalLossW, c->activeMassKg, c->materialCostIndex,
                c->efficiencyPercent, c->impedancePercent);
        }
    }

    if (optimization) {
        fprintf(out, "\nOPTIMAL DESIGN COMPARISON - TEXTBOOK CRITERIA\n--------------------------------------------\n");
        fprintf(out, "%d evenly spaced attempts from %d evaluated (%d feasible). Original serial numbers retained.\n",
            optimization->sampleCount, optimization->evaluatedDesigns, optimization->feasibleDesigns);
        fprintf(out, "Efficiency: full load, 0.85 PF, configured loss reference temperature. Mass: model active mass, excluding tank/oil.\n");
        fprintf(out, " Sn   Bm(T)  J(A/mm2) H/W     eff(%%)    kg/kVA    I0/I2(%%)   tank(m3) Status\n");
        for (int i = 0; i < optimization->sampleCount; i++) {
            const OptimizationCandidate *c = &optimization->samples[i];
            fprintf(out, "%3d   %.3f  %.3f    %.3f  ", c->serialNumber, c->Bm, c->currentDensityTarget, c->windowAspectRatio);
            if (c->calculated) fprintf(out, "%8.4f  %8.4f  %8.4f   %8.4f ", c->comparisonEfficiencyPercent, c->specificMassKgKva, c->noLoadCurrentPercent, c->tankVolumeM3);
            else fprintf(out, "       -         -         -          - ");
            fprintf(out, "%s\n", c->feasible ? "FEASIBLE" : c->constraintFailures);
        }
        static const char *criteria[] = {"Maximum efficiency (full load, 0.85 PF)", "Minimum active kg/kVA", "Minimum I0/I2 (%)", "Minimum tank volume (m3)"};
        fprintf(out, "\nSelections use ALL feasible attempts; exact ties choose the earliest serial.\n");
        for (int i = 0; i < OPTIMIZATION_CRITERIA_COUNT; i++) {
            const OptimizationCandidate *c = &optimization->criteriaWinners[i];
            if (!c->serialNumber) { fprintf(out, "%s: no feasible variant.\n", criteria[i]); continue; }
            double value = i == 0 ? c->comparisonEfficiencyPercent : i == 1 ? c->specificMassKgKva : i == 2 ? c->noLoadCurrentPercent : c->tankVolumeM3;
            fprintf(out, "%s: select variant Sn %d (%.6f).\n", criteria[i], c->serialNumber, value);
        }
        if (optimization->recommendedIndex >= 0) {
            const OptimizationCandidate *c = &optimization->candidates[optimization->recommendedIndex];
            fprintf(out, "Balanced Pareto: select variant Sn %d (score %.6f; loss %.3f W, active mass %.3f kg, cost index %.3f).\n",
                c->serialNumber, c->balanceScore, c->totalLossW, c->activeMassKg, c->materialCostIndex);
        }
        fprintf(out, "Feasible means the implemented hard constraints passed; local benchmark compliance is assessed separately.\n");
    }

    fprintf(out, "\nNOTES\n-----\n");
    fprintf(out, "- Local loss values are interpolated from Kenya Power's 2025 11/0.420 kV tables.\n");
    fprintf(out, "- The project LV rating is configurable and may differ slightly from the 420 V benchmark.\n");
    fprintf(out, "- This analytical model supports design comparison; type tests and detailed mechanical,\n");
    fprintf(out, "  dielectric, short-circuit and thermal verification remain necessary before manufacture.\n");
}

void printTransformerResults(const Transformer *tx)
{
    printf("%.0f kVA %.0f/%.0f V %s-%s: ", tx->input.KVA, tx->input.HV, tx->input.LV,
        connectionName(tx->input.hvConnection), connectionName(tx->input.lvConnection));
    if (tx->completedSections & SECTION_PERFORMANCE)
        printf("loss %.3f kW, efficiency %.3f%%, impedance %.3f%%",
            tx->performance.ptFL, tx->performance.cases[0].efficiency, tx->performance.Ez * 100.0);
    if (tx->completedSections & SECTION_TANK)
        printf(", cooled rise %.2f C, %d tubes", tx->tank.TrWithTubes, tx->tank.Nt);
    printf("\n");
}

int writeTextReport(const Transformer *tx, const OptimizationSet *optimization, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    writeTextStream(tx, optimization, file);
    fclose(file);
    return 0;
}

static void jsonString(FILE *out, const char *value)
{
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\') { fputc('\\', out); fputc(*p, out); }
        else if (*p == '\n') fputs("\\n", out);
        else if (*p == '\r') fputs("\\r", out);
        else if (*p == '\t') fputs("\\t", out);
        else if (*p >= 32) fputc(*p, out);
    }
    fputc('"', out);
}

static void jsonSectionNames(FILE *out, unsigned sections)
{
    static const char *names[] = {"frame", "no-load", "lv", "hv", "performance", "tank"};
    int first = 1;
    fputc('[', out);
    for (int i = 0; i < 6; i++) {
        if (!(sections & (1u << i))) continue;
        if (!first) fputc(',', out);
        jsonString(out, names[i]);
        first = 0;
    }
    fputc(']', out);
}

static void jsonOptimizationCandidate(FILE *out, const OptimizationCandidate *c)
{
    fprintf(out, "{\"serialNumber\":%d,\"calculated\":%s,\"feasible\":%s,\"constraintFailures\":", c->serialNumber, c->calculated ? "true" : "false", c->feasible ? "true" : "false");
    jsonString(out, c->constraintFailures);
    fprintf(out, ",\"bm\":%.10g,\"currentDensityTarget\":%.10g,\"windowAspectRatio\":%.10g", c->Bm, c->currentDensityTarget, c->windowAspectRatio);
    if (!c->calculated) {
        fputs(",\"comparisonEfficiencyPercent\":null,\"specificMassKgKva\":null,\"noLoadCurrentPercent\":null,\"tankVolumeM3\":null}", out);
        return;
    }
    fprintf(out, ",\"totalLossW\":%.10g,\"activeMassKg\":%.10g,\"materialCostIndex\":%.10g,\"efficiencyPercent\":%.10g,\"impedancePercent\":%.10g,\"temperatureRiseC\":%.10g,\"benchmarkPassCount\":%d,\"balanceScore\":%.10g,\"comparisonEfficiencyPercent\":%.10g,\"specificMassKgKva\":%.10g,\"noLoadCurrentPercent\":%.10g,\"tankVolumeM3\":%.10g}",
        c->totalLossW, c->activeMassKg, c->materialCostIndex, c->efficiencyPercent,
        c->impedancePercent, c->temperatureRiseC, c->benchmarkPassCount, c->balanceScore,
        c->comparisonEfficiencyPercent, c->specificMassKgKva, c->noLoadCurrentPercent, c->tankVolumeM3);
}

int writeJsonStream(const Transformer *tx, const OptimizationSet *optimization, FILE *out)
{
    char code[16];
    connectionCode(tx, code, sizeof(code));
    fprintf(out, "{\n  \"meta\":{\"mode\":"); jsonString(out, runModeName(tx->runMode));
    fprintf(out, ",\"requestedSections\":"); jsonSectionNames(out, tx->requestedSections);
    fprintf(out, ",\"completedSections\":"); jsonSectionNames(out, tx->completedSections);
    fprintf(out, ",\"benchmark\":\"Kenya Power 2025, 11/0.420 kV\"},\n");
    fprintf(out, "  \"input\":{\"kva\":%.8g,\"hvVoltage\":%.8g,\"lvVoltage\":%.8g,\"phases\":%d,\"frequency\":%.8g,",
        tx->input.KVA, tx->input.HV, tx->input.LV, tx->input.Ph, tx->input.f);
    fprintf(out, "\"hvConnection\":"); jsonString(out, connectionName(tx->input.hvConnection));
    fprintf(out, ",\"lvConnection\":"); jsonString(out, connectionName(tx->input.lvConnection));
    fprintf(out, ",\"connectionCode\":"); jsonString(out, code);
    fprintf(out, ",\"vectorClock\":%d,\"cooling\":", tx->input.vectorClock); jsonString(out, tx->input.cooling);
    fprintf(out, ",\"targetTemperatureRiseC\":%.8g,\"referenceTemperatureC\":%.8g,\"coreFluxDensityT\":%.8g,\"averageCurrentDensity\":%.8g,\"windowAspectRatio\":%.8g},\n",
        tx->input.TRP, tx->input.referenceTemperatureC, tx->input.Bm, tx->input.cdav, tx->input.windowAspectRatio);
    fprintf(out, "  \"assumptions\":{\"coreStepFactor\":%.8g,\"emfValueFactor\":%.8g,\"stackingFactor\":%.8g,\"yokeAreaFactor\":%.8g,\"yokeWidthFactor\":%.8g,",
        tx->input.k, tx->input.K, tx->input.ki, tx->input.yokeAreaFactor, tx->input.yokeWidthFactor);
    fprintf(out, "\"windowSpaceMultiplier\":%.8g,\"dimensionRoundingM\":%.8g,\"coreLossReferenceFluxT\":%.8g,\"coreLossReferenceWKg\":%.8g,",
        tx->input.windowSpaceMultiplier, tx->input.dimensionRoundingM,
        tx->input.coreLossReferenceFluxT, tx->input.coreLossReferenceWKg);
    fprintf(out, "\"yokeLossReferenceFluxT\":%.8g,\"yokeLossReferenceWKg\":%.8g,\"lossCurveExponent\":%.8g,\"ironLossBuildFactor\":%.8g,",
        tx->input.yokeLossReferenceFluxT, tx->input.yokeLossReferenceWKg,
        tx->input.lossCurveExponent, tx->input.ironLossBuildFactor);
    fprintf(out, "\"coreATPerM\":%.8g,\"yokeATPerM\":%.8g,\"excitationBuildFactor\":%.8g,\"conductorInsulationMm\":%.8g,",
        tx->input.coreATPerMeter, tx->input.yokeATPerMeter,
        tx->input.excitationBuildFactor, tx->input.conductorInsulationMm);
    fprintf(out, "\"copperResistivity20\":%.8g,\"copperTemperatureConstant\":%.8g,\"copperStrayLossFactor\":%.8g,\"copperDensityKgM3\":%.8g,\"steelDensityKgM3\":%.8g,\"oilDensityKgM3\":%.8g,",
        tx->input.copperResistivity20, tx->input.copperTemperatureConstant,
        tx->input.copperStrayLossFactor, tx->input.density_cu, tx->input.density_fe, tx->input.density_oil);
    fprintf(out, "\"lvWindingHeightFraction\":%.8g,\"lvEdgeFactor\":%.8g,\"lvInterTurnInsulationMm\":%.8g,\"lvEndInsulationMm\":%.8g,\"lvRadialInsulationMm\":%.8g,",
        tx->input.lvWindingHeightFraction, tx->input.lvEdgeFactor, tx->input.lvInterTurnInsulationMm,
        tx->input.lvEndInsulationMm, tx->input.lvRadialInsulationMm);
    fprintf(out, "\"hvWindingHeightFraction\":%.8g,\"hvEdgeFactor\":%.8g,\"hvInterCoilInsulationMm\":%.8g,\"hvEndRingMm\":%.8g,\"hvEndInsulationMm\":%.8g,",
        tx->input.hvWindingHeightFraction, tx->input.hvEdgeFactor, tx->input.hvInterCoilInsulationMm,
        tx->input.hvEndRingMm, tx->input.hvEndInsulationMm);
    fprintf(out, "\"coreToLvOilDuctMm\":%.8g,\"coreToLvCylinderMm\":%.8g,\"lvFormerToWindingDuctMm\":%.8g,\"lvToHvOilDuctMm\":%.8g,\"lvToHvCylinderMm\":%.8g,\"hvFormerToWindingDuctMm\":%.8g,",
        tx->input.coreToLvOilDuctMm, tx->input.coreToLvCylinderMm,
        tx->input.lvFormerToWindingDuctMm, tx->input.lvToHvOilDuctMm,
        tx->input.lvToHvCylinderMm, tx->input.hvFormerToWindingDuctMm);
    fprintf(out, "\"plainTankDissipationWm2C\":%.8g,\"tubeCoefficientWm2C\":%.8g,\"tubeEffectiveness\":%.8g,\"tankPlateThicknessM\":%.8g,",
        tx->input.plainTankDissipation, tx->input.tubeCoefficient,
        tx->input.tubeEffectiveness, tx->input.tankPlateThicknessM);
    fprintf(out, "\"copperCostIndex\":%.8g,\"coreSteelCostIndex\":%.8g,\"tankSteelCostIndex\":%.8g,\"oilCostIndex\":%.8g},\n",
        tx->input.copperCostIndex, tx->input.coreSteelCostIndex,
        tx->input.tankSteelCostIndex, tx->input.oilCostIndex);
    fprintf(out, "  \"summary\":{\"ironLossW\":%.8g,\"loadLossW\":%.8g,\"totalLossW\":%.8g,\"fullLoadEfficiencyPercent\":%.8g,\"impedancePercent\":%.8g,\"regulation85Percent\":%.8g,\"temperatureRiseC\":%.8g,\"coolingTubes\":%d,\"activeMassKg\":%.8g,\"shippingMassKg\":%.8g},\n",
        tx->magneticFrame.Pi * 1000.0, tx->performance.pcuT * 1000.0,
        tx->performance.ptFL * 1000.0, tx->performance.cases[0].efficiency,
        tx->performance.Ez * 100.0, tx->performance.Reg85 * 100.0,
        tx->tank.TrWithTubes, tx->tank.Nt, tx->tank.Wtot, tx->tankDerived.Wship);
    fprintf(out, "  \"sections\":{\n");
    const MagneticFrame *f = &tx->magneticFrame;
    const NoLoadCurrent *n = &tx->noLoadCurrent;
    const Winding *lv = &tx->lv;
    const Winding *hv = &tx->hv;
    const Performance *p = &tx->performance;
    const Tank *t = &tx->tank;
    fprintf(out, "    \"frame\":{\"voltsPerTurn\":%.8g,\"netCoreAreaM2\":%.8g,\"grossCoreAreaM2\":%.8g,\"coreDiameterM\":%.8g,\"windowSpaceFactor\":%.8g,\"windowAreaM2\":%.8g,",
        f->Et, f->Ai, f->Ac, f->d, f->kw, f->Aw);
    fprintf(out, "\"coreLengthM\":%.8g,\"centreDistanceM\":%.8g,\"windowRatio\":%.8g,\"yokeLengthM\":%.8g,\"yokeGrossAreaM2\":%.8g,\"yokeWidthM\":%.8g,\"yokeHeightM\":%.8g,",
        f->L, f->D, f->windowRatio, f->W, f->Ay, f->by, f->hy);
    fprintf(out, "\"coreFluxDensityT\":%.8g,\"yokeFluxDensityT\":%.8g,\"coreLossWPerKg\":%.8g,\"yokeLossWPerKg\":%.8g,\"coreMassKg\":%.8g,\"yokeMassKg\":%.8g,",
        tx->input.Bm, f->By, f->WpKgC, f->WpKgY, f->KgC, f->KgY);
    fprintf(out, "\"coreIronLossW\":%.8g,\"yokeIronLossW\":%.8g,\"ironLossW\":%.8g,\"totalIronMassKg\":%.8g,\"totalIronVolumeM3\":%.8g},\n",
        f->PiC, f->PiY, f->Pi * 1000.0, f->KgC + f->KgY,
        (f->KgC + f->KgY) / tx->input.density_fe);
    fprintf(out, "    \"noLoad\":{\"coreATPerM\":%.8g,\"yokeATPerM\":%.8g,\"coreAmpereTurns\":%.8g,\"yokeAmpereTurns\":%.8g,\"totalATPerPhase\":%.8g,",
        n->atC, n->atY, n->ATC, n->ATY, n->ATpPh);
    fprintf(out, "\"lvTurns\":%.8g,\"lvPhaseVoltageV\":%.8g,\"lvPhaseCurrentA\":%.8g,\"wattfulCurrentA\":%.8g,\"magnetizingCurrentA\":%.8g,\"noLoadCurrentA\":%.8g,\"noLoadCurrentPercent\":%.8g},\n",
        n->T2, windingPhaseVoltage(tx->input.LV, tx->input.lvConnection), n->I2,
        n->Iw, n->Im, n->I0, n->I0byI2);
    fprintf(out, "    \"lv\":{\"phaseVoltageV\":%.8g,\"turns\":%.8g,\"phaseCurrentA\":%.8g,\"availableWindingHeightMm\":%.8g,\"turnsRadially\":%.8g,\"turnsAxially\":%.8g,",
        lv->phaseVoltage, lv->T, lv->I, lv->ALW, lv->Tr, lv->Ta);
    fprintf(out, "\"parallelStrands\":%.8g,\"axialStrands\":%.8g,\"radialStrands\":%.8g,\"spacePerTurnMm\":%.8g,\"strandWidthMm\":%.8g,\"strandThicknessMm\":%.8g,",
        lv->stP, lv->NstA, lv->NstR, lv->ALT, lv->stW, lv->stT);
    fprintf(out, "\"nominalConductorAreaMm2\":%.8g,\"conductorAreaMm2\":%.8g,\"currentDensity\":%.8g,\"activeHeightMm\":%.8g,\"occupiedHeightMm\":%.8g,\"axialSlackMm\":%.8g,",
        lv->stW * lv->stT * lv->stP, lv->a, lv->cd,
        lv->activeAxialLength, lv->ALWx, lv->SlkAx);
    fprintf(out, "\"radialWidthMm\":%.8g,\"innerDiameterMm\":%.8g,\"outerDiameterMm\":%.8g,\"meanTurnLengthM\":%.8g,\"conductorLengthM\":%.8g,\"copperVolumeM3\":%.8g,\"copperMassKg\":%.8g,",
        lv->rw, lv->di, lv->do_, lv->Lmt, lv->Lcu, lv->Vcu, lv->Wcu);
    fprintf(out, "\"resistance20Ohm\":%.8g,\"resistanceReferenceOhm\":%.8g,\"copperLoss20Kw\":%.8g,\"copperLossReferenceKw\":%.8g},\n",
        lv->r20, lv->rReference, lv->pcu20, lv->pcuReference);
    fprintf(out, "    \"hv\":{\"phaseVoltageV\":%.8g,\"turns\":%.8g,\"phaseCurrentA\":%.8g,\"availableWindingHeightMm\":%.8g,\"coils\":%.8g,\"middleCoilTurns\":%.8g,\"endCoilTurns\":%.8g,",
        hv->phaseVoltage, hv->T, hv->I, hv->ALW, hv->Ta, hv->middleCoilTurns, hv->endCoilTurns);
    fprintf(out, "\"parallelStrands\":%.8g,\"axialStrands\":%.8g,\"radialStrands\":%.8g,\"spacePerCoilMm\":%.8g,\"strandWidthMm\":%.8g,\"strandThicknessMm\":%.8g,",
        hv->stP, hv->NstA, hv->NstR, hv->ALT, hv->stW, hv->stT);
    fprintf(out, "\"nominalConductorAreaMm2\":%.8g,\"conductorAreaMm2\":%.8g,\"currentDensity\":%.8g,\"activeHeightMm\":%.8g,\"occupiedHeightMm\":%.8g,\"axialSlackMm\":%.8g,",
        hv->stW * hv->stT, hv->a, hv->cd, hv->activeAxialLength, hv->ALWx, hv->SlkAx);
    fprintf(out, "\"radialWidthMm\":%.8g,\"innerDiameterMm\":%.8g,\"outerDiameterMm\":%.8g,\"adjacentClearanceMm\":%.8g,\"meanTurnLengthM\":%.8g,\"conductorLengthM\":%.8g,\"copperVolumeM3\":%.8g,\"copperMassKg\":%.8g,",
        hv->rw, hv->di, hv->do_, f->D * 1000.0 - hv->do_, hv->Lmt, hv->Lcu, hv->Vcu, hv->Wcu);
    fprintf(out, "\"resistance20Ohm\":%.8g,\"resistanceReferenceOhm\":%.8g,\"copperLoss20Kw\":%.8g,\"copperLossReferenceKw\":%.8g},\n",
        hv->r20, hv->rReference, hv->pcu20, hv->pcuReference);
    fprintf(out, "    \"performance\":{\"lvCopperLoss20Kw\":%.8g,\"hvCopperLoss20Kw\":%.8g,\"lvCopperLossReferenceKw\":%.8g,\"hvCopperLossReferenceKw\":%.8g,",
        lv->pcu20, hv->pcu20, lv->pcuReference, hv->pcuReference);
    fprintf(out, "\"copperLoss20Kw\":%.8g,\"copperLossReferenceKw\":%.8g,\"ironLossKw\":%.8g,\"totalLossKw\":%.8g,\"maxEfficiencyLoadKva\":%.8g,\"maxEfficiencyPercent\":%.8g,",
        p->pcuT20, p->pcuT, f->Pi, p->ptFL, p->Ldmxef, p->efmx);
    fprintf(out, "\"meanTurnLengthM\":%.8g,\"coilLengthM\":%.8g,\"ampereTurnsPerPhase\":%.8g,\"resistancePu\":%.8g,\"reactancePu\":%.8g,\"impedancePu\":%.8g,\"regulation85Pu\":%.8g,\"regulationUnityPu\":%.8g,\"cases\":[",
        p->Lmt, p->Lc, p->AT, p->Er, p->Ex, p->Ez, p->Reg85, p->RegUPF);
    for (int i = 0; i < PERFORMANCE_CASES; i++) {
        const PerformanceCase *c = &tx->performance.cases[i];
        fprintf(out, "%s{\"powerFactor\":%.8g,\"loadPu\":%.8g,\"lossKw\":%.8g,\"outputKw\":%.8g,\"inputKw\":%.8g,\"efficiencyPercent\":%.8g}",
            i ? "," : "", c->pf, c->loadPU, c->losses, c->output, c->input, c->efficiency);
    }
    fprintf(out, "]},\n");
    fprintf(out, "    \"tank\":{\"lengthClearanceM\":%.8g,\"widthClearanceM\":%.8g,\"heightClearanceM\":%.8g,\"tubeDiameterM\":%.8g,\"tubeHeightM\":%.8g,\"targetRiseC\":%.8g,",
        t->dL, t->dB, t->dH, t->Dct, t->Hct, t->TRP);
    fprintf(out, "\"lengthM\":%.8g,\"widthM\":%.8g,\"heightM\":%.8g,\"volumeM3\":%.8g,\"surfaceAreaM2\":%.8g,\"plainRiseC\":%.8g,\"cooledRiseC\":%.8g,",
        t->Lt, t->bt, t->ht, t->Vt, t->St, t->Tr, t->TrWithTubes);
    fprintf(out, "\"tubeAreaM2\":%.8g,\"requiredTubeAreaM2\":%.8g,\"coolingTubes\":%d,\"hvCopperMassKg\":%.8g,\"lvCopperMassKg\":%.8g,\"ironMassKg\":%.8g,",
        t->At, t->CAt, t->Nt, t->Wcu1, t->Wcu2, t->Wiron);
    fprintf(out, "\"activeMassKg\":%.8g,\"specificMassKgKva\":%.8g,\"steelMassKg\":%.8g,\"oilVolumeM3\":%.8g,\"oilMassKg\":%.8g,\"shippingMassKg\":%.8g,\"materialCostIndex\":%.8g}\n",
        t->Wtot, t->KgPkva, tx->tankDerived.Wsteel, tx->tankDerived.Voil,
        tx->tankDerived.Woil, tx->tankDerived.Wship, tx->tankDerived.materialCostIndex);
    fprintf(out, "  },\n  \"billOfMaterials\":[");
    fprintf(out, "{\"item\":\"HV copper\",\"massKg\":%.8g},", tx->tank.Wcu1);
    fprintf(out, "{\"item\":\"LV copper\",\"massKg\":%.8g},", tx->tank.Wcu2);
    fprintf(out, "{\"item\":\"Active core steel\",\"massKg\":%.8g},", tx->tank.Wiron);
    fprintf(out, "{\"item\":\"Fabricated tank steel\",\"massKg\":%.8g},", tx->tankDerived.Wsteel);
    fprintf(out, "{\"item\":\"Insulating oil\",\"massKg\":%.8g}],\n  \"evaluations\":[", tx->tankDerived.Woil);
    for (int i = 0; i < tx->evaluationCount; i++) {
        const Evaluation *e = &tx->evaluations[i];
        if (i) fputc(',', out);
        fprintf(out, "{\"id\":"); jsonString(out, e->id);
        fprintf(out, ",\"label\":"); jsonString(out, e->label);
        fprintf(out, ",\"value\":%.8g,\"unit\":", e->value); jsonString(out, e->unit);
        fprintf(out, ",\"limitLow\":%.8g,\"limitHigh\":%.8g,\"status\":", e->limitLow, e->limitHigh); jsonString(out, statusName(e->status));
        fprintf(out, ",\"source\":"); jsonString(out, e->source);
        fprintf(out, ",\"meaning\":"); jsonString(out, e->meaning);
        fprintf(out, ",\"verdict\":"); jsonString(out, e->verdict);
        fputc('}', out);
    }
    fprintf(out, "],\n  \"optimization\":");
    if (!optimization) {
        fputs("null\n", out);
    } else {
        fprintf(out, "{\"evaluatedDesigns\":%d,\"feasibleDesigns\":%d,\"paretoCount\":%d,\"recommendedIndex\":%d,\"comparisonPowerFactor\":0.85,\"comparisonLoadPu\":1,\"candidates\":[",
            optimization->evaluatedDesigns, optimization->feasibleDesigns, optimization->paretoCount, optimization->recommendedIndex);
        for (int i = 0; i < optimization->count; i++) {
            if (i) fputc(',', out);
            jsonOptimizationCandidate(out, &optimization->candidates[i]);
        }
        fputs("],\"samples\":[", out);
        for (int i = 0; i < optimization->sampleCount; i++) {
            if (i) fputc(',', out);
            jsonOptimizationCandidate(out, &optimization->samples[i]);
        }
        fputs("],\"criteriaWinners\":{", out);
        static const char *keys[] = {"efficiency", "specificMass", "noLoadCurrent", "tankVolume"};
        for (int i = 0; i < OPTIMIZATION_CRITERIA_COUNT; i++) {
            if (i) fputc(',', out);
            jsonString(out, keys[i]); fputc(':', out);
            if (optimization->criteriaWinners[i].serialNumber) jsonOptimizationCandidate(out, &optimization->criteriaWinners[i]);
            else fputs("null", out);
        }
        fputs("}}\n", out);
    }
    fputs("}\n", out);
    return ferror(out) ? -1 : 0;
}

int writeJsonReport(const Transformer *tx, const OptimizationSet *optimization, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file) return -1;
    int result = writeJsonStream(tx, optimization, file);
    fclose(file);
    return result;
}
