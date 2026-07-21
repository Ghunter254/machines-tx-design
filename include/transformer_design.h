#ifndef TRANSFORMER_DESIGN_H
#define TRANSFORMER_DESIGN_H

#include <stdbool.h>
#include <stddef.h>

#define TARGET_TEMP_RISE_C      50.0

#define OIL_DENSITY_KG_M3       860.0
#define STEEL_DENSITY_KG_M3     7850.0
#define COPPER_DENSITY_KG_M3    8960.0

#define PLAIN_TANK_DISSIPATION  12.5
#define TUBE_COEFFICIENT        6.5
#define TUBE_EFFECTIVENESS      1.35

#define PERFORMANCE_CASES       4

#define RESET   "\x1b[0m"
#define BOLD    "\x1b[1m"
#define DIM     "\x1b[2m"

#define FG_WHITE  "\x1b[97m"
#define FG_CYAN   "\x1b[36m"
#define FG_GREEN  "\x1b[32m"
#define FG_YELLOW "\x1b[33m"

#define INNER_W   58

typedef enum
{
    FIELD_DOUBLE,
    FIELD_INT
} FieldType;

typedef struct
{
    const char *key;
    size_t offset;
    FieldType type;
    const char *unit;
} FieldEntry;

typedef struct
{
    /* Transformer Rating */
    double KVA;         /* Apparent Power (kVA) */
    double HV;          /* High Voltage Rating (V) */
    double LV;          /* Low Voltage Rating (V) */
    double Ph;          /* Number of Phases */
    double f;           /* Frequency (Hz) */

    /* Design Requirements */
    double TRP;         /* Target Permissible Temp Rise (°C) */

    /* Material Properties */
    double density_cu;  /* Copper Density (kg/m³) */
    double density_fe;  /* Steel Density (kg/m³) */
    double density_oil; /* Oil Density (kg/m³) */

    /* Core Design Assumptions */
    double k;           /* Core Stepping Factor */
    double K;           /* Volts/Turn Value Factor */
    double Bm;          /* Max Flux Density in Core (T) */
    double cdav;        /* Average Current Density (A/mm²) */
    double kw;          /* Window Space Factor */
    double ki;          /* Iron Stacking Factor */

    /* Cooling Tubes Assumptions */
    double Dct;         /* Cooling Tube Diameter (m) */
    double Hct;         /* Cooling Tube Height (m) */

    /* Tank Clearances Assumptions */
    double dL;          /* Length-wise Clearance (m) */
    double dB;          /* Width-wise Clearance (m) */
    double dH;          /* Height-wise Clearance (m) */
} DesignInputs;

typedef struct
{
    /* Electrical */
    double Et;          /* Volts per Turn (V/turn) */

    /* Core Geometry */
    double Ai;          /* Net Core Cross-sectional Area (m²) */
    double d;           /* Core Limb Diameter (m) */
    double Ac;          /* Gross Core Cross-sectional Area (m²) */
    double Aw;          /* Window Area (m²) */
    double L;           /* Length of Core / Window Height (m) */
    double D;           /* Distance Between Core Centres (m) */
    double W;           /* Length of Yoke (m) */
    double Ay;          /* Yoke Cross-sectional Area (m²) */
    double by;          /* Width of Yoke (m) */
    double hy;          /* Height of Yoke (m) */

    /* Core Loss */
    double WpKgC;       /* Core Loss per kg (W/kg) */
    double KgC;         /* Core Weight (kg) */
    double PiC;         /* Core Iron Loss (W) */

    /* Yoke Loss */
    double By;          /* Flux Density in Yoke (T) */
    double WpKgY;       /* Yoke Loss per kg (W/kg) */
    double KgY;         /* Yoke Weight (kg) */
    double PiY;         /* Yoke Iron Loss (W) */

    /* Final */
    double Pi;          /* Total Iron Loss (kW) */
} MagneticFrame;

typedef struct
{
    /* Magnetising Force */
    double atC;         /* AT/m for Core Steel */
    double atY;         /* AT/m for Yoke Steel */

    /* Ampere Turns */
    double ATC;         /* Total AT for Core */
    double ATY;         /* Total AT for Yoke */
    double ATpPh;       /* Total AT per Phase */

    /* LV Side Reference */
    double T2;          /* Number of LV Turns */
    double I2;          /* LV Phase Current (A) */

    /* No-Load Current Components */
    double Iw;          /* Wattful Current (A) */
    double Im;          /* Magnetising Current (A) */
    double I0;          /* Total No-Load Current (A) */
    double I0byI2;      /* No-Load Current Percentage (%) */
} NoLoadCurrent;

typedef struct
{
    /* Turns Layout */
    double T;           /* Number of Turns (T1 or T2) */
    double Tr;          /* Turns Radially (T2r or x2/cR) */
    double Ta;          /* Turns Axially (T2a or T1a/AxC) */

    /* Current & Area */
    double I;           /* Phase Current (I1 or I2) (A) */
    double a;           /* Conductor Cross-sectional Area (a1 or a2) (mm²) */
    double cd;          /* Current Density (cdHV or cdLV) (A/mm²) */

    /* Axial Space Available & Occupied */
    double ALW;         /* Space Available for Turns (mm) */
    double ALT;         /* Space Allocation per Turn (mm) */
    double ALWx;        /* Total Space Occupied Axially (ALWx / AxLw) (mm) */
    double SlkAx;       /* Axial Slack in Winding (SlkLVax / SlkHVax) (mm) */

    /* Conductor Stranding */
    double stP;         /* Parallel Strands */
    double NstA;        /* Axial Strands (NstA or cA) */
    double NstR;        /* Radial Strands (NstR or cR) */
    double stW;         /* Bare Strand Width (mm) */
    double stT;         /* Bare Strand Thickness (mm) */

    /* Winding Structural Dimensions */
    double rw;          /* Radial Width of Winding (rwLV / rwHV) (mm) */
    double di;          /* Inner Diameter (di1 / di2) (mm) */
    double do_;         /* Outer Diameter (do1 / do2) (mm) - do_ to avoid C keyword */
    double Lmt;         /* Mean Turn Length (Lmt1 / Lmt2) (m) */

    /* Copper Metrics & Extensions */
    double Lcu;         /* Total Conductor Length (m) - Reporting Extension */
    double Vcu;         /* Total Conductor Volume (m³) - Reporting Extension */
    double Wcu;         /* Copper Weight (Wcu1 / Wcu2) (kg) */

    /* Electrical Performance */
    double r;           /* Resistance per Phase (r1 or r2) (Ω/mΩ) */
    double pcu;         /* Copper Loss (pcu1 or pcu2) (kW) */
} Winding;

typedef struct
{
    double pf;          /* Power Factor */
    double loadPU;      /* Load (Per Unit) */
    double losses;      /* Total Loss at Load (kW) */
    double output;      /* Output Power (kW) */
    double input;       /* Input Power (kW) */
    double efficiency;  /* Efficiency (%) */
} PerformanceCase;

typedef struct
{
    /* Total Losses */
    double pcuT;        /* Total Winding Copper Loss (kW) */
    double ptFL;        /* Total Full Load Loss (kW) */

    /* Maximum Efficiency */
    double Ldmxef;      /* Load at Maximum Efficiency (kVA) */
    double efmx;        /* Maximum Efficiency (%) */

    /* Reactance Parameters */
    double Lmt;         /* Overall Mean Turn Length (m) */
    double Lc;          /* Active Coil Length / Stack Height (m) */
    double AT;          /* Ampere Turns per Phase */
    double Er;          /* Per Unit Resistance (pu) */
    double Ex;          /* Per Unit Reactance (pu) */
    double Ez;          /* Per Unit Impedance (pu) */

    /* Voltage Regulation */
    double Reg85;       /* Regulation at 0.85 PF (pu) */
    double RegUPF;      /* Regulation at Unity PF (pu) */

    /* Tabulated Performance Cases */
    PerformanceCase cases[PERFORMANCE_CASES];
} Performance;

typedef struct
{
    /* Clearances */
    double dL;          /* Length-wise Clearance (m) */
    double dB;          /* Width-wise Clearance (m) */
    double dH;          /* Height-wise Clearance (m) */

    /* Tank Dimensions */
    double Lt;          /* Tank Length (m) */
    double bt;          /* Tank Width (m) */
    double ht;          /* Tank Height (m) */
    double Vt;          /* Tank Volume (m³) */
    double St;          /* Tank Cooling Surface Area (m²) */

    /* Thermal Evaluation */
    double Tr;          /* Tank Temperature Rise without Tubes (°C) */
    double TRP;         /* Permissible Temperature Rise (°C) */

    /* Cooling Tubes */
    double Dct;         /* Cooling Tube Diameter (m) */
    double Hct;         /* Cooling Tube Height (m) */
    double At;          /* Area of One Cooling Tube (m²) */
    double CAt;         /* Required Tube Cooling Area (m²) */
    int    Nt;          /* Number of Cooling Tubes Required */

    /* Phase 6 Component Weights Rollup */
    double Wcu1;        /* HV Winding Copper Weight (kg) */
    double Wcu2;        /* LV Winding Copper Weight (kg) */
    double Wiron;       /* Total Iron Core Weight (kg) */

    /* Final Weight Rollup */
    double Wtot;        /* Total Transformer Weight (kg) */
    double KgPkva;      /* Specific Weight (kg/kVA) */
} Tank;

typedef struct
{
    double Wsteel;      /* Tank Steel Plate Weight (kg) - Reporting Extension */
    double Voil;        /* Required Oil Volume (m³) - Reporting Extension */
    double Woil;        /* Oil Weight (kg) - Reporting Extension */
    double Wship;       /* Total Shipping Weight (kg) - Reporting Extension */
} TankDerived;

typedef struct
{
    DesignInputs    input;
    MagneticFrame   magneticFrame;
    NoLoadCurrent   noLoadCurrent;
    Winding         lv;
    Winding         hv;
    Performance     performance;
    Tank            tank;
    TankDerived     tankDerived;
} Transformer;

void printTransformerResults(const Transformer *tx);

int tread(
    Transformer *tx,
    const char *filename,
    const char **keys,
    int nkeys
);

int twrite(
    Transformer *tx,
    const char *filename,
    const char **keys,
    int nkeys
);

#endif /* TRANSFORMER_DESIGN_H */