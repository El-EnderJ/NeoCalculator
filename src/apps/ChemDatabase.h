#pragma once

#include <cstdint>

namespace chem {

enum class ChemCategory : uint8_t {
    NonMetal, NobleGas, AlkaliMetal, AlkalineEarth, Metalloid,
    Halogen, TransitionMetal, PostTransitionMetal, Lanthanide, Actinide, Unknown
};

struct ElementData {
    uint8_t atomicNumber;
    char symbol[4];
    char name[16];
    float mass;
    ChemCategory category;
    float electronegativity;
    char electronConfig[24];
};

namespace CC {
    constexpr auto NM = ChemCategory::NonMetal;
    constexpr auto NG = ChemCategory::NobleGas;
    constexpr auto AM = ChemCategory::AlkaliMetal;
    constexpr auto AE = ChemCategory::AlkalineEarth;
    constexpr auto ML = ChemCategory::Metalloid;
    constexpr auto HL = ChemCategory::Halogen;
    constexpr auto TM = ChemCategory::TransitionMetal;
    constexpr auto PT = ChemCategory::PostTransitionMetal;
    constexpr auto LN = ChemCategory::Lanthanide;
    constexpr auto AC = ChemCategory::Actinide;
    constexpr auto UK = ChemCategory::Unknown;
} // namespace CC

constexpr ElementData ELEMENTS[118] = {
    {  1, "H",   "Hydrogen",           1.008f, CC::NM,  2.2f, "1s1"},
    {  2, "He",  "Helium",             4.003f, CC::NG,  0.0f, "1s2"},
    {  3, "Li",  "Lithium",            6.941f, CC::AM, 0.98f, "[He] 2s1"},
    {  4, "Be",  "Beryllium",          9.012f, CC::AE, 1.57f, "[He] 2s2"},
    {  5, "B",   "Boron",              10.81f, CC::ML, 2.04f, "[He] 2s2 2p1"},
    {  6, "C",   "Carbon",            12.011f, CC::NM, 2.55f, "[He] 2s2 2p2"},
    {  7, "N",   "Nitrogen",          14.007f, CC::NM, 3.04f, "[He] 2s2 2p3"},
    {  8, "O",   "Oxygen",            15.999f, CC::NM, 3.44f, "[He] 2s2 2p4"},
    {  9, "F",   "Fluorine",          18.998f, CC::HL, 3.98f, "[He] 2s2 2p5"},
    { 10, "Ne",  "Neon",               20.18f, CC::NG,  0.0f, "[He] 2s2 2p6"},
    { 11, "Na",  "Sodium",             22.99f, CC::AM, 0.93f, "[Ne] 3s1"},
    { 12, "Mg",  "Magnesium",         24.305f, CC::AE, 1.31f, "[Ne] 3s2"},
    { 13, "Al",  "Aluminum",          26.982f, CC::PT, 1.61f, "[Ne] 3s2 3p1"},
    { 14, "Si",  "Silicon",           28.085f, CC::ML,  1.9f, "[Ne] 3s2 3p2"},
    { 15, "P",   "Phosphorus",        30.974f, CC::NM, 2.19f, "[Ne] 3s2 3p3"},
    { 16, "S",   "Sulfur",             32.06f, CC::NM, 2.58f, "[Ne] 3s2 3p4"},
    { 17, "Cl",  "Chlorine",           35.45f, CC::HL, 3.16f, "[Ne] 3s2 3p5"},
    { 18, "Ar",  "Argon",             39.948f, CC::NG,  0.0f, "[Ne] 3s2 3p6"},
    { 19, "K",   "Potassium",         39.098f, CC::AM, 0.82f, "[Ar] 4s1"},
    { 20, "Ca",  "Calcium",           40.078f, CC::AE,  1.0f, "[Ar] 4s2"},
    { 21, "Sc",  "Scandium",          44.956f, CC::TM, 1.36f, "[Ar] 3d1 4s2"},
    { 22, "Ti",  "Titanium",          47.867f, CC::TM, 1.54f, "[Ar] 3d2 4s2"},
    { 23, "V",   "Vanadium",          50.942f, CC::TM, 1.63f, "[Ar] 3d3 4s2"},
    { 24, "Cr",  "Chromium",          51.996f, CC::TM, 1.66f, "[Ar] 3d5 4s1"},
    { 25, "Mn",  "Manganese",         54.938f, CC::TM, 1.55f, "[Ar] 3d5 4s2"},
    { 26, "Fe",  "Iron",              55.845f, CC::TM, 1.83f, "[Ar] 3d6 4s2"},
    { 27, "Co",  "Cobalt",            58.933f, CC::TM, 1.88f, "[Ar] 3d7 4s2"},
    { 28, "Ni",  "Nickel",            58.693f, CC::TM, 1.91f, "[Ar] 3d8 4s2"},
    { 29, "Cu",  "Copper",            63.546f, CC::TM,  1.9f, "[Ar] 3d10 4s1"},
    { 30, "Zn",  "Zinc",               65.38f, CC::TM, 1.65f, "[Ar] 3d10 4s2"},
    { 31, "Ga",  "Gallium",           69.723f, CC::PT, 1.81f, "[Ar] 3d10 4s2 4p1"},
    { 32, "Ge",  "Germanium",          72.63f, CC::ML, 2.01f, "[Ar] 3d10 4s2 4p2"},
    { 33, "As",  "Arsenic",           74.922f, CC::ML, 2.18f, "[Ar] 3d10 4s2 4p3"},
    { 34, "Se",  "Selenium",          78.971f, CC::NM, 2.55f, "[Ar] 3d10 4s2 4p4"},
    { 35, "Br",  "Bromine",           79.904f, CC::HL, 2.96f, "[Ar] 3d10 4s2 4p5"},
    { 36, "Kr",  "Krypton",           83.798f, CC::NG,  3.0f, "[Ar] 3d10 4s2 4p6"},
    { 37, "Rb",  "Rubidium",          85.468f, CC::AM, 0.82f, "[Kr] 5s1"},
    { 38, "Sr",  "Strontium",          87.62f, CC::AE, 0.95f, "[Kr] 5s2"},
    { 39, "Y",   "Yttrium",           88.906f, CC::TM, 1.22f, "[Kr] 4d1 5s2"},
    { 40, "Zr",  "Zirconium",         91.224f, CC::TM, 1.33f, "[Kr] 4d2 5s2"},
    { 41, "Nb",  "Niobium",           92.906f, CC::TM,  1.6f, "[Kr] 4d4 5s1"},
    { 42, "Mo",  "Molybdenum",         95.95f, CC::TM, 2.16f, "[Kr] 4d5 5s1"},
    { 43, "Tc",  "Technetium",          98.0f, CC::TM,  1.9f, "[Kr] 4d5 5s2"},
    { 44, "Ru",  "Ruthenium",         101.07f, CC::TM,  2.2f, "[Kr] 4d7 5s1"},
    { 45, "Rh",  "Rhodium",           102.91f, CC::TM, 2.28f, "[Kr] 4d8 5s1"},
    { 46, "Pd",  "Palladium",         106.42f, CC::TM,  2.2f, "[Kr] 4d10"},
    { 47, "Ag",  "Silver",            107.87f, CC::TM, 1.93f, "[Kr] 4d10 5s1"},
    { 48, "Cd",  "Cadmium",           112.41f, CC::TM, 1.69f, "[Kr] 4d10 5s2"},
    { 49, "In",  "Indium",            114.82f, CC::PT, 1.78f, "[Kr] 4d10 5s2 5p1"},
    { 50, "Sn",  "Tin",               118.71f, CC::PT, 1.96f, "[Kr] 4d10 5s2 5p2"},
    { 51, "Sb",  "Antimony",          121.76f, CC::ML, 2.05f, "[Kr] 4d10 5s2 5p3"},
    { 52, "Te",  "Tellurium",          127.6f, CC::ML,  2.1f, "[Kr] 4d10 5s2 5p4"},
    { 53, "I",   "Iodine",             126.9f, CC::HL, 2.66f, "[Kr] 4d10 5s2 5p5"},
    { 54, "Xe",  "Xenon",             131.29f, CC::NG,  2.6f, "[Kr] 4d10 5s2 5p6"},
    { 55, "Cs",  "Cesium",            132.91f, CC::AM, 0.79f, "[Xe] 6s1"},
    { 56, "Ba",  "Barium",            137.33f, CC::AE, 0.89f, "[Xe] 6s2"},
    { 57, "La",  "Lanthanum",         138.91f, CC::LN,  1.1f, "[Xe] 5d1 6s2"},
    { 58, "Ce",  "Cerium",            140.12f, CC::LN, 1.12f, "[Xe] 4f1 5d1 6s2"},
    { 59, "Pr",  "Praseodymium",      140.91f, CC::LN, 1.13f, "[Xe] 4f3 6s2"},
    { 60, "Nd",  "Neodymium",         144.24f, CC::LN, 1.14f, "[Xe] 4f4 6s2"},
    { 61, "Pm",  "Promethium",         145.0f, CC::LN, 1.13f, "[Xe] 4f5 6s2"},
    { 62, "Sm",  "Samarium",          150.36f, CC::LN, 1.17f, "[Xe] 4f6 6s2"},
    { 63, "Eu",  "Europium",          151.96f, CC::LN,  1.2f, "[Xe] 4f7 6s2"},
    { 64, "Gd",  "Gadolinium",        157.25f, CC::LN,  1.2f, "[Xe] 4f7 5d1 6s2"},
    { 65, "Tb",  "Terbium",           158.93f, CC::LN,  1.1f, "[Xe] 4f9 6s2"},
    { 66, "Dy",  "Dysprosium",         162.5f, CC::LN, 1.22f, "[Xe] 4f10 6s2"},
    { 67, "Ho",  "Holmium",           164.93f, CC::LN, 1.23f, "[Xe] 4f11 6s2"},
    { 68, "Er",  "Erbium",            167.26f, CC::LN, 1.24f, "[Xe] 4f12 6s2"},
    { 69, "Tm",  "Thulium",           168.93f, CC::LN, 1.25f, "[Xe] 4f13 6s2"},
    { 70, "Yb",  "Ytterbium",         173.05f, CC::LN,  1.1f, "[Xe] 4f14 6s2"},
    { 71, "Lu",  "Lutetium",          174.97f, CC::LN, 1.27f, "[Xe] 4f14 5d1 6s2"},
    { 72, "Hf",  "Hafnium",           178.49f, CC::TM,  1.3f, "[Xe] 4f14 5d2 6s2"},
    { 73, "Ta",  "Tantalum",          180.95f, CC::TM,  1.5f, "[Xe] 4f14 5d3 6s2"},
    { 74, "W",   "Tungsten",          183.84f, CC::TM, 2.36f, "[Xe] 4f14 5d4 6s2"},
    { 75, "Re",  "Rhenium",           186.21f, CC::TM,  1.9f, "[Xe] 4f14 5d5 6s2"},
    { 76, "Os",  "Osmium",            190.23f, CC::TM,  2.2f, "[Xe] 4f14 5d6 6s2"},
    { 77, "Ir",  "Iridium",           192.22f, CC::TM,  2.2f, "[Xe] 4f14 5d7 6s2"},
    { 78, "Pt",  "Platinum",          195.08f, CC::TM, 2.28f, "[Xe] 4f14 5d9 6s1"},
    { 79, "Au",  "Gold",              196.97f, CC::TM, 2.54f, "[Xe] 4f14 5d10 6s1"},
    { 80, "Hg",  "Mercury",           200.59f, CC::TM,  2.0f, "[Xe] 4f14 5d10 6s2"},
    { 81, "Tl",  "Thallium",          204.38f, CC::PT, 1.62f, "[Xe] 4f14 5d10 6s2 6p1"},
    { 82, "Pb",  "Lead",               207.2f, CC::PT, 1.87f, "[Xe] 4f14 5d10 6s2 6p2"},
    { 83, "Bi",  "Bismuth",           208.98f, CC::PT, 2.02f, "[Xe] 4f14 5d10 6s2 6p3"},
    { 84, "Po",  "Polonium",           209.0f, CC::PT,  2.0f, "[Xe] 4f14 5d10 6s2 6p4"},
    { 85, "At",  "Astatine",           210.0f, CC::HL,  2.2f, "[Xe] 4f14 5d10 6s2 6p5"},
    { 86, "Rn",  "Radon",              222.0f, CC::NG,  2.2f, "[Xe] 4f14 5d10 6s2 6p6"},
    { 87, "Fr",  "Francium",           223.0f, CC::AM,  0.7f, "[Rn] 7s1"},
    { 88, "Ra",  "Radium",             226.0f, CC::AE,  0.9f, "[Rn] 7s2"},
    { 89, "Ac",  "Actinium",           227.0f, CC::AC,  1.1f, "[Rn] 6d1 7s2"},
    { 90, "Th",  "Thorium",           232.04f, CC::AC,  1.3f, "[Rn] 6d2 7s2"},
    { 91, "Pa",  "Protactinium",      231.04f, CC::AC,  1.5f, "[Rn] 5f2 6d1 7s2"},
    { 92, "U",   "Uranium",           238.03f, CC::AC, 1.38f, "[Rn] 5f3 6d1 7s2"},
    { 93, "Np",  "Neptunium",          237.0f, CC::AC, 1.36f, "[Rn] 5f4 6d1 7s2"},
    { 94, "Pu",  "Plutonium",          244.0f, CC::AC, 1.28f, "[Rn] 5f6 7s2"},
    { 95, "Am",  "Americium",          243.0f, CC::AC,  1.3f, "[Rn] 5f7 7s2"},
    { 96, "Cm",  "Curium",             247.0f, CC::AC,  1.3f, "[Rn] 5f7 6d1 7s2"},
    { 97, "Bk",  "Berkelium",          247.0f, CC::AC,  1.3f, "[Rn] 5f9 7s2"},
    { 98, "Cf",  "Californium",        251.0f, CC::AC,  1.3f, "[Rn] 5f10 7s2"},
    { 99, "Es",  "Einsteinium",        252.0f, CC::AC,  1.3f, "[Rn] 5f11 7s2"},
    {100, "Fm",  "Fermium",            257.0f, CC::AC,  1.3f, "[Rn] 5f12 7s2"},
    {101, "Md",  "Mendelevium",        258.0f, CC::AC,  1.3f, "[Rn] 5f13 7s2"},
    {102, "No",  "Nobelium",           259.0f, CC::AC,  1.3f, "[Rn] 5f14 7s2"},
    {103, "Lr",  "Lawrencium",         266.0f, CC::AC,  1.3f, "[Rn] 5f14 7s2 7p1"},
    {104, "Rf",  "Rutherfordium",      267.0f, CC::TM,  0.0f, "[Rn] 5f14 6d2 7s2"},
    {105, "Db",  "Dubnium",            268.0f, CC::TM,  0.0f, "[Rn] 5f14 6d3 7s2"},
    {106, "Sg",  "Seaborgium",         269.0f, CC::TM,  0.0f, "[Rn] 5f14 6d4 7s2"},
    {107, "Bh",  "Bohrium",            270.0f, CC::TM,  0.0f, "[Rn] 5f14 6d5 7s2"},
    {108, "Hs",  "Hassium",            277.0f, CC::TM,  0.0f, "[Rn] 5f14 6d6 7s2"},
    {109, "Mt",  "Meitnerium",         278.0f, CC::UK,  0.0f, "[Rn] 5f14 6d7 7s2"},
    {110, "Ds",  "Darmstadtium",       281.0f, CC::UK,  0.0f, "[Rn] 5f14 6d9 7s1"},
    {111, "Rg",  "Roentgenium",        282.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s1"},
    {112, "Cn",  "Copernicium",        285.0f, CC::TM,  0.0f, "[Rn] 5f14 6d10 7s2"},
    {113, "Nh",  "Nihonium",           286.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p1"},
    {114, "Fl",  "Flerovium",          289.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p2"},
    {115, "Mc",  "Moscovium",          290.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p3"},
    {116, "Lv",  "Livermorium",        293.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p4"},
    {117, "Ts",  "Tennessine",         294.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p5"},
    {118, "Og",  "Oganesson",          294.0f, CC::UK,  0.0f, "[Rn] 5f14 6d10 7s2 7p6"}
};

constexpr int8_t TABLE_LAYOUT[10][18] = {
    {   1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,    2},
    {   3,    4,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,    5,    6,    7,    8,    9,   10},
    {  11,   12,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   13,   14,   15,   16,   17,   18},
    {  19,   20,   21,   22,   23,   24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36},
    {  37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51,   52,   53,   54},
    {  55,   56,   -1,   72,   73,   74,   75,   76,   77,   78,   79,   80,   81,   82,   83,   84,   85,   86},
    {  87,   88,   -1,  104,  105,  106,  107,  108,  109,  110,  111,  112,  113,  114,  115,  116,  117,  118},
    {  -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1},
    {  -1,   -1,   57,   58,   59,   60,   61,   62,   63,   64,   65,   66,   67,   68,   69,   70,   71,   -1},
    {  -1,   -1,   89,   90,   91,   92,   93,   94,   95,   96,   97,   98,   99,  100,  101,  102,  103,   -1}
};

// D-Pad navigation: {UP, DOWN, LEFT, RIGHT} atomic numbers (0 = no neighbor)
constexpr uint8_t NAV_LUT[118][4] = {
    {  0,   3,   0,   2},  //   1 H
    {  0,  10,   1,   0},  //   2 He
    {  1,  11,   0,   4},  //   3 Li
    {  0,  12,   3,   5},  //   4 Be
    {  0,  13,   4,   6},  //   5 B
    {  0,  14,   5,   7},  //   6 C
    {  0,  15,   6,   8},  //   7 N
    {  0,  16,   7,   9},  //   8 O
    {  0,  17,   8,  10},  //   9 F
    {  2,  18,   9,   0},  //  10 Ne
    {  3,  19,   0,  12},  //  11 Na
    {  4,  20,  11,  13},  //  12 Mg
    {  5,  31,  12,  14},  //  13 Al
    {  6,  32,  13,  15},  //  14 Si
    {  7,  33,  14,  16},  //  15 P
    {  8,  34,  15,  17},  //  16 S
    {  9,  35,  16,  18},  //  17 Cl
    { 10,  36,  17,   0},  //  18 Ar
    { 11,  37,   0,  20},  //  19 K
    { 12,  38,  19,  21},  //  20 Ca
    {  0,  39,  20,  22},  //  21 Sc
    {  0,  40,  21,  23},  //  22 Ti
    {  0,  41,  22,  24},  //  23 V
    {  0,  42,  23,  25},  //  24 Cr
    {  0,  43,  24,  26},  //  25 Mn
    {  0,  44,  25,  27},  //  26 Fe
    {  0,  45,  26,  28},  //  27 Co
    {  0,  46,  27,  29},  //  28 Ni
    {  0,  47,  28,  30},  //  29 Cu
    {  0,  48,  29,  31},  //  30 Zn
    { 13,  49,  30,  32},  //  31 Ga
    { 14,  50,  31,  33},  //  32 Ge
    { 15,  51,  32,  34},  //  33 As
    { 16,  52,  33,  35},  //  34 Se
    { 17,  53,  34,  36},  //  35 Br
    { 18,  54,  35,   0},  //  36 Kr
    { 19,  55,   0,  38},  //  37 Rb
    { 20,  56,  37,  39},  //  38 Sr
    { 21,   0,  38,  40},  //  39 Y
    { 22,  72,  39,  41},  //  40 Zr
    { 23,  73,  40,  42},  //  41 Nb
    { 24,  74,  41,  43},  //  42 Mo
    { 25,  75,  42,  44},  //  43 Tc
    { 26,  76,  43,  45},  //  44 Ru
    { 27,  77,  44,  46},  //  45 Rh
    { 28,  78,  45,  47},  //  46 Pd
    { 29,  79,  46,  48},  //  47 Ag
    { 30,  80,  47,  49},  //  48 Cd
    { 31,  81,  48,  50},  //  49 In
    { 32,  82,  49,  51},  //  50 Sn
    { 33,  83,  50,  52},  //  51 Sb
    { 34,  84,  51,  53},  //  52 Te
    { 35,  85,  52,  54},  //  53 I
    { 36,  86,  53,   0},  //  54 Xe
    { 37,  87,   0,  56},  //  55 Cs
    { 38,  88,  55,  72},  //  56 Ba
    {  0,  89,   0,  58},  //  57 La
    { 72,  90,  57,  59},  //  58 Ce
    { 73,  91,  58,  60},  //  59 Pr
    { 74,  92,  59,  61},  //  60 Nd
    { 75,  93,  60,  62},  //  61 Pm
    { 76,  94,  61,  63},  //  62 Sm
    { 77,  95,  62,  64},  //  63 Eu
    { 78,  96,  63,  65},  //  64 Gd
    { 79,  97,  64,  66},  //  65 Tb
    { 80,  98,  65,  67},  //  66 Dy
    { 81,  99,  66,  68},  //  67 Ho
    { 82, 100,  67,  69},  //  68 Er
    { 83, 101,  68,  70},  //  69 Tm
    { 84, 102,  69,  71},  //  70 Yb
    { 85, 103,  70,   0},  //  71 Lu
    { 40, 104,  56,  73},  //  72 Hf
    { 41, 105,  72,  74},  //  73 Ta
    { 42, 106,  73,  75},  //  74 W
    { 43, 107,  74,  76},  //  75 Re
    { 44, 108,  75,  77},  //  76 Os
    { 45, 109,  76,  78},  //  77 Ir
    { 46, 110,  77,  79},  //  78 Pt
    { 47, 111,  78,  80},  //  79 Au
    { 48, 112,  79,  81},  //  80 Hg
    { 49, 113,  80,  82},  //  81 Tl
    { 50, 114,  81,  83},  //  82 Pb
    { 51, 115,  82,  84},  //  83 Bi
    { 52, 116,  83,  85},  //  84 Po
    { 53, 117,  84,  86},  //  85 At
    { 54, 118,  85,   0},  //  86 Rn
    { 55,   0,   0,  88},  //  87 Fr
    { 56,   0,  87, 104},  //  88 Ra
    { 57,   0,   0,  90},  //  89 Ac
    { 58,   0,  89,  91},  //  90 Th
    { 59,   0,  90,  92},  //  91 Pa
    { 60,   0,  91,  93},  //  92 U
    { 61,   0,  92,  94},  //  93 Np
    { 62,   0,  93,  95},  //  94 Pu
    { 63,   0,  94,  96},  //  95 Am
    { 64,   0,  95,  97},  //  96 Cm
    { 65,   0,  96,  98},  //  97 Bk
    { 66,   0,  97,  99},  //  98 Cf
    { 67,   0,  98, 100},  //  99 Es
    { 68,   0,  99, 101},  // 100 Fm
    { 69,   0, 100, 102},  // 101 Md
    { 70,   0, 101, 103},  // 102 No
    { 71,   0, 102,   0},  // 103 Lr
    { 72,  58,  88, 105},  // 104 Rf
    { 73,  59, 104, 106},  // 105 Db
    { 74,  60, 105, 107},  // 106 Sg
    { 75,  61, 106, 108},  // 107 Bh
    { 76,  62, 107, 109},  // 108 Hs
    { 77,  63, 108, 110},  // 109 Mt
    { 78,  64, 109, 111},  // 110 Ds
    { 79,  65, 110, 112},  // 111 Rg
    { 80,  66, 111, 113},  // 112 Cn
    { 81,  67, 112, 114},  // 113 Nh
    { 82,  68, 113, 115},  // 114 Fl
    { 83,  69, 114, 116},  // 115 Mc
    { 84,  70, 115, 117},  // 116 Lv
    { 85,  71, 116, 118},  // 117 Ts
    { 86,   0, 117,   0}   // 118 Og
};

} // namespace chem
