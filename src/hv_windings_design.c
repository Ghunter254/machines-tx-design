#include "../include/transformer_design.h"
#include <stdio.h>
#include <math.h>

static void calculateHVBaseAndLayout(Transformer *tx);
static void calculateHVConductorDimensions(Transformer *tx);
static void calculateHVAxialAndRadialBuild(Transformer *tx);
static void calculateHVDiametersAndLosses(Transformer *tx);

void designHVWinding(Transformer *tx)
{
    calculateHVBaseAndLayout(tx);
    calculateHVConductorDimensions(tx);
    calculateHVAxialAndRadialBuild(tx);
    calculateHVDiametersAndLosses(tx);
}

static void calculateHVBaseAndLayout(Transformer *tx) 
{
    // Calculate:
    // - No. of turns/ph (T1)
    // - Phase current in HV Wdg (I1)
    // - Disc layout turn distribution and strands (x1, cA, x2, x3)
}

static void calculateHVConductorDimensions(Transformer *tx) 
{
    // Calculate:
    // - Length available for winding (ALW)
    // - Space per coil (ALPC) and space for each strand (ALPC1)
    // - Strand width (stW1)
    // - Initial Current density (cdHV) and CS area of conductor (a1)
    // - Thickness of strand (stT1)
    // - Corrected CS area of strand (a1) and corrected Current density (cdHV)
}

static void calculateHVAxialAndRadialBuild(Transformer *tx) 
{
    // Calculate:
    // - Axial length of strands (aLc)
    // - Radial width of Winding (rwHV)
    // - Axial length occupied by all strands (AxLw)
    // - Total Axial Length with end rings/insulation (AxL)
    // - Slack Available axially (SlkHVax)
}

static void calculateHVDiametersAndLosses(Transformer *tx) 
{
    // Calculate:
    // - Inside dia of HV wdg (di1)
    // - Outer dia of HV winding (do1)
    // - Mean length of HV turns (Lmt1)
    // - Res of HV wdg/ph (r1)
    // - Copper loss in HV Wdg (pcu1)
}