/*
 *
 * Transformer Design Software
 *
 * Project:
 * 630kVA, 11kV / 415 V Distribution Transformer
 *
 * Institution:
 * Strathmore University
 *
 * CD:
 * Ghost
 *
 * Description:
 * This is the main entry point for the transformer design Software
 *
 */



#include <stdio.h>
#include<stdlib.h>
#include <string.h>

#include "../include/transformer_design.h"
#include "../include/tank_design.h"
#include "../include/main.h"

int main(void) {

    Transformer tx = {0};
    printf("=====================================================\n");
    printf("     TRANSFORMER DESIGN SOFTWARE\n");
    printf("=====================================================\n");
    printf("Rating      : 630 kVA\n");
    printf("Voltage     : 11 kV / 415 V\n");
    printf("Cooling     : ONAN\n");
    printf("Frequency   : 50 Hz\n");
    printf("=====================================================\n\n");

    printf("Loading configuration...\n");
    
    if (tread(&tx, "../data/config.txt", NULL, 0) != 0) {
        printf("Error: Failed to load config file.\n");
        return EXIT_FAILURE;
    }
    
    printf("Configuration loaded successfully. \n\n");

    designTank(&tx);
    printTransformerResults(&tx);
    twrite(&tx, "../data/output.txt", NULL, 0);

    return EXIT_SUCCESS;
    

}


static void hline(const char *left, const char *fill, const char *right)
{
    printf("%s%s", FG_CYAN, left);
    for (int i = 0; i < INNER_W; i++) printf("%s", fill);
    printf("%s%s\n", right, RESET);
}

static void printCentered(const char *text, const char *color)
{
    int len = (int)strlen(text);
    int pad = (INNER_W - len) / 2;
    int padRight = INNER_W - len - pad;
    printf("%s\xe2\x94\x82%*s%s%s%s%*s\xe2\x94\x82%s\n",
           FG_CYAN, pad, "", color, text, RESET FG_CYAN, padRight, "", RESET);
}

static void printSection(const char *title)
{
    char buf[64];
    snprintf(buf, sizeof(buf), " %s", title);
    printf("%s\xe2\x94\x9c", FG_CYAN);
    for (int i = 0; i < INNER_W; i++) printf("\xe2\x94\x80");
    printf("\xe2\x94\xa4%s\n", RESET);
    printf("%s\xe2\x94\x82%s%s%-*s%s%s\xe2\x94\x82%s\n",
           FG_CYAN, RESET, BOLD FG_YELLOW, INNER_W, buf, RESET, FG_CYAN, RESET);
}

static void row(const char *label, double value, const char *unit)
{
    char content[80], valbuf[32];
    snprintf(valbuf, sizeof(valbuf), "%.2f %s", value, unit);
    snprintf(content, sizeof(content), "  %-28s %26s", label, valbuf);
    printf("%s\xe2\x94\x82%s%-*s%s\xe2\x94\x82%s\n",
           FG_CYAN, FG_WHITE, INNER_W, content, FG_CYAN, RESET);
}

static void rowInt(const char *label, int value, const char *unit)
{
    char content[80], valbuf[32];
    snprintf(valbuf, sizeof(valbuf), "%d %s", value, unit);
    snprintf(content, sizeof(content), "  %-28s %26s", label, valbuf);
    printf("%s\xe2\x94\x82%s%-*s%s\xe2\x94\x82%s\n",
           FG_CYAN, FG_WHITE, INNER_W, content, FG_CYAN, RESET);
}

static void rowHighlight(const char *label, double value, const char *unit)
{
    char content[80], valbuf[32];
    snprintf(valbuf, sizeof(valbuf), "%.2f %s", value, unit);
    snprintf(content, sizeof(content), "  %-28s %26s", label, valbuf);
    printf("%s\xe2\x94\x82%s%s%-*s%s%s\xe2\x94\x82%s\n",
           FG_CYAN, BOLD, FG_GREEN, INNER_W, content, RESET, FG_CYAN, RESET);
}

static void blank(void)
{
    printf("%s\xe2\x94\x82%*s\xe2\x94\x82%s\n", FG_CYAN, INNER_W, "", RESET);
}
void printTransformerResults(const Transformer *tx)
{
    char title[64];

    printf("\n");

    hline("╔", "═", "╗");

    printCentered("TRANSFORMER DESIGN REPORT", BOLD FG_WHITE);

    snprintf(title,
             sizeof(title),
             "%.0f kVA  |  %.0f / %.0f V  |  %.0f Hz",
             tx->input.KVA,
             tx->input.HV,
             tx->input.LV,
             tx->input.f);

    printCentered(title, DIM FG_WHITE);

    hline("╠", "═", "╣");

    /*--------------------------------------------------*/
    /* Magnetic Frame & Core Geometry                   */
    /*--------------------------------------------------*/

    printSection("MAGNETIC FRAME & CORE");

    row("Core Limb Diameter (d)",      tx->magneticFrame.d,    "m");
    row("Window Height / Length (L)",  tx->magneticFrame.L,    "m");
    row("Distance Between Centers (D)",tx->magneticFrame.D,    "m");
    row("Net Core Area (Ai)",          tx->magneticFrame.Ai,   "m^2");
    row("Window Area (Aw)",            tx->magneticFrame.Aw,   "m^2");
    row("Volts per Turn (Et)",         tx->magneticFrame.Et,   "V/turn");
    row("Core Iron Loss (PiC)",        tx->magneticFrame.PiC,  "W");
    row("Yoke Iron Loss (PiY)",        tx->magneticFrame.PiY,  "W");
    rowHighlight("Total Iron Loss (Pi)",tx->magneticFrame.Pi,  "kW");

    /*--------------------------------------------------*/
    /* No-Load Current Excitation                       */
    /*--------------------------------------------------*/

    printSection("NO-LOAD CURRENT");

    row("Total MMF per Phase",         tx->noLoadCurrent.ATpPh, "AT");
    row("Wattful Current (Iw)",        tx->noLoadCurrent.Iw,    "A");
    row("Magnetizing Current (Im)",    tx->noLoadCurrent.Im,    "A");
    row("No-Load Current (I0)",        tx->noLoadCurrent.I0,    "A");
    rowHighlight("I0 / I2 Ratio",      tx->noLoadCurrent.I0byI2,"%");

    /*--------------------------------------------------*/
    /* LV Winding                                       */
    /*--------------------------------------------------*/

    printSection("LV WINDING");

    row("Turns (T2)",                  tx->lv.T,               "");
    row("Inner Diameter (di2)",        tx->lv.di,              "mm");
    row("Outer Diameter (do2)",        tx->lv.do_,             "mm");
    row("Axial Height (ALWx)",         tx->lv.ALWx,            "mm");
    row("Current Density (cdLV)",      tx->lv.cd,              "A/mm^2");
    row("Conductor Length (Lcu2)",     tx->lv.Lcu,             "m");
    rowHighlight("LV Copper Weight (Wcu2)", tx->lv.Wcu,        "kg");

    /*--------------------------------------------------*/
    /* HV Winding                                       */
    /*--------------------------------------------------*/

    printSection("HV WINDING");

    row("Turns (T1)",                  tx->hv.T,               "");
    row("Inner Diameter (di1)",        tx->hv.di,              "mm");
    row("Outer Diameter (do1)",        tx->hv.do_,             "mm");
    row("Axial Height (ALWx)",         tx->hv.ALWx,            "mm");
    row("Current Density (cdHV)",      tx->hv.cd,              "A/mm^2");
    row("Conductor Length (Lcu1)",     tx->hv.Lcu,             "m");
    rowHighlight("HV Copper Weight (Wcu1)", tx->hv.Wcu,        "kg");

    /*--------------------------------------------------*/
    /* Performance                                      */
    /*--------------------------------------------------*/

    printSection("PERFORMANCE");

    row("Copper Loss (pcuT)",          tx->performance.pcuT,   "kW");
    row("Iron Loss (Pi)",              tx->magneticFrame.Pi,   "kW");
    row("Total Full Load Loss (ptFL)", tx->performance.ptFL,   "kW");

    row("Resistance (Er)",             tx->performance.Er,     "pu");
    row("Reactance (Ex)",              tx->performance.Ex,     "pu");
    row("Impedance (Ez)",              tx->performance.Ez,     "pu");

    row("Regulation (0.85 PF)",        tx->performance.Reg85 * 100.0, "%");
    row("Regulation (UPF)",            tx->performance.RegUPF * 100.0, "%");

    rowHighlight("Maximum Efficiency", tx->performance.efmx,   "%");

    /*--------------------------------------------------*/
    /* Tank                                             */
    /*--------------------------------------------------*/

    printSection("TANK");

    row("Length (Lt)",                 tx->tank.Lt,            "m");
    row("Width (bt)",                  tx->tank.bt,            "m");
    row("Height (ht)",                 tx->tank.ht,            "m");

    row("Volume (Vt)",                 tx->tank.Vt,            "m^3");
    row("Surface Area (St)",           tx->tank.St,            "m^2");

    row("Plain Tank Temp Rise (Tr)",   tx->tank.Tr,            "°C");
    row("Required Tube Area (CAt)",    tx->tank.CAt,           "m^2");

    rowInt("Cooling Tubes (Nt)",       tx->tank.Nt,            "");

    /*--------------------------------------------------*/
    /* Weights                                          */
    /*--------------------------------------------------*/

    printSection("WEIGHTS ROLLUP");

    row("Core Iron Weight (Wiron)",    tx->tank.Wiron,         "kg");
    row("HV Copper Weight (Wcu1)",     tx->tank.Wcu1,          "kg");
    row("LV Copper Weight (Wcu2)",     tx->tank.Wcu2,          "kg");

    row("Tank Steel Plate (Wsteel)",   tx->tankDerived.Wsteel, "kg");
    row("Insulating Oil (Woil)",       tx->tankDerived.Woil,   "kg");

    rowHighlight("Active Weight (Wtot)",
                 tx->tank.Wtot,
                 "kg");

    rowHighlight("Shipping Weight (Wship)",
                 tx->tankDerived.Wship,
                 "kg");

    rowHighlight("Specific Weight (KgPkva)",
                 tx->tank.KgPkva,
                 "kg/kVA");

    blank();

    hline("╚", "═", "╝");

    printf("\nProgram completed successfully.\n");
}