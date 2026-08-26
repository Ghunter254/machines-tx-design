#include "../include/transformer_design.h"
#include <stdio.h>
#include <math.h>

static void calculateCoreCrossSection(Transformer *tx);
static void calculateWindowDimensions(Transformer *tx);
static void calculateYokeDimensions(Transformer *tx);
static void  calculateIronLosses(Transformer *tx);


void designFrame(Transformer *tx)
{
    calculateCoreCrossSection(tx);
    calculateWindowDimensions(tx);
    calculateYokeDimensions(tx);
    calculateIronLosses(tx);

    printf("Magnetic frame design completed successfully.\n");
}

static void calculateCoreCrossSection(Transformer *tx) 
{
    tx->magneticFrame.Ai = (tx->magneticFrame.Et / (4.44 * tx->input.f * tx->input.Bm ));
    tx->magneticFrame.d = sqrt(tx->magneticFrame.Ai / tx->input.k);
    tx->magneticFrame.Ai = tx->input.k * (tx->magneticFrame.d * tx->magneticFrame.d);
    tx->magneticFrame.Et = 4.44 * tx->input.f * tx->input.Bm * tx->magneticFrame.Ai;

}

static void calculateWindowDimensions(Transformer *tx) 
{
    tx->input.kw = 10 / (30 + tx->input.KVA / 1000);
    tx->magneticFrame.Aw = (tx->input.KVA * 1000) / (3.33 * tx->input.f * tx->input.Bm * tx->input.kw * tx->input.kw * tx->input.cdav * 10e6 * tx->magneticFrame.Ai);
    tx->magneticFrame.L = tx->magneticFrame.Aw / (tx->magneticFrame.D - tx->magneticFrame.d);
    tx->magneticFrame.D = (tx->magneticFrame.Aw/ tx->magneticFrame.L) + tx->magneticFrame.d ;

}
