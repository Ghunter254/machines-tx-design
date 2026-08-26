#include "../include/transformer_design.h"
#include <stdio.h>
#include <math.h>

static void calculateLVAxialDimensions(Transformer *tx);
static void calculateLVConductorAndRadial(Transformer *tx);
static void calculateLVDiametersAndLength(Transformer *tx);
static void calculateLVLosses(Transformer *tx);

void designLVWinding(Transformer *tx)
{
    calculateLVAxialDimensions(tx);
    calculateLVConductorAndRadial(tx);
    calculateLVDiametersAndLength(tx);
    calculateLVLosses(tx);
}

static void calculateLVAxialDimensions(Transformer *tx) 
{
    // Calculate:
    // - Space available for turns (ALW)
    // - Axial turns / Length-wise (T2a)
    // - Space/turn (ALT)
    // - Width of each strand (stW)
    // - Space occupied length-wise (ALWx)
    // - Slack in length (SlkLVax)
}

static void calculateLVConductorAndRadial(Transformer *tx) 
{
    // Calculate:
    // - CS area of conductor (a2)
    // - Current Density (cdLV)
    // - Radial Width of the winding (rwLV)
}

static void calculateLVDiametersAndLength(Transformer *tx) 
{
    // Calculate:
    // - Inner dia of LV wdg (di2)
    // - Outer dia of LV wdg (do2)
    // - Mean length of LV winding (Lmt2)
}

static void calculateLVLosses(Transformer *tx) 
{
    // Calculate:
    // - Res of LV wdg/ph (r2)
    // - Copper loss in LV wdg (pcu2)
}