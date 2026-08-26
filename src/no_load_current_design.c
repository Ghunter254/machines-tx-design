#include "../include/transformer_design.h"
#include <stdio.h>
#include <math.h>

static void calculateAmpereTurns(Transformer *tx);
static void calculateLVBaseReferences(Transformer *tx);
static void calculateExcitationCurrents(Transformer *tx);

void designNoLoadCurrent(Transformer *tx)
{
    calculateAmpereTurns(tx);
    calculateLVBaseReferences(tx);
    calculateExcitationCurrents(tx);
}

static void calculateAmpereTurns(Transformer *tx) 
{
    // Calculate:
    // - AT for Core (ATC)
    // - AT for Yoke (ATY)
    // - Total AT/phase (ATpPh)
}

static void calculateLVBaseReferences(Transformer *tx) 
{
    // Calculate:
    // - No. of Turns in LV wdg (T2)
    // - Phase Current in LV Wdg (I2)
}

static void calculateExcitationCurrents(Transformer *tx) 
{
    // Calculate:
    // - Wattful Current (Iw)
    // - Magnetizing current (Im)
    // - No load Current (I0)
    // - Ratio of I0/I2 (I0byI2)
}