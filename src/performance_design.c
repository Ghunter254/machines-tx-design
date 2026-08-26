#include "../include/transformer_design.h"
#include <stdio.h>
#include <math.h>

static void calculateTotalLossesAndMaxEff(Transformer *tx);
static void calculateEfficiencyTable(Transformer *tx);
static void calculateImpedanceParameters(Transformer *tx);
static void calculateVoltageRegulation(Transformer *tx);

void designPerformance(Transformer *tx)
{
    calculateTotalLossesAndMaxEff(tx);
    calculateEfficiencyTable(tx);
    calculateImpedanceParameters(tx);
    calculateVoltageRegulation(tx);
}

static void calculateTotalLossesAndMaxEff(Transformer *tx) 
{
    // Calculate:
    // - Total Wdg Copper losses (pcuT) (Assuming 5% stray losses)
    // - Total losses on Full Load (ptFL)
    // - Load for max. Efficiency (Ldmxef)
    // - Max Efficiency (efmx)
}

static void calculateEfficiencyTable(Transformer *tx) 
{
    // Calculate Total Loss, Output, Input, and Efficiency for the 4 variants
    // Store these in the tx->performance.cases array:
    // - Case 0: pf = 1.0, Load(pu) = 1.0
    // - Case 1: pf = 0.85, Load(pu) = 1.0
    // - Case 2: pf = 0.85, Load(pu) = 0.75
    // - Case 3: pf = 0.85, Load(pu) = 0.5
}

static void calculateImpedanceParameters(Transformer *tx) 
{
    // Calculate:
    // - Mean Turn Length (Lmt)
    // - Length of coil (Lc)
    // - AT/ph (AT)
    // - P.U. Reactance (Ex)
    // - P.U. Resistance (Er)
    // - P.U. Impedance (Ez)
}

static void calculateVoltageRegulation(Transformer *tx) 
{
    // Calculate:
    // - Regulation at 0.85pf and FL (Reg85)
    // - Regulation at UPF and FL (RegUPF)
}