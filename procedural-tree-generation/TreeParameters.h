#pragma once

#include "Config.h"

struct LeafParameters {
    int    Count;
    // Shape ratio index for leaf scale along parent
    int    ScaleShape;
    float  Scale;
    float  ScaleX;
    float  StemLen;
    float  BotAngle;
    float  MidAngle;
    float  TopAngle;
    float  SideOffset;
    int    Lobes;
    float  LobeAngle;
    float  LobeFalloff;
    float4 Color;
    float  Translucency;

    float StrandAngle;
    float StrandCurve;
    float StrandStep;
    float StrandOffset;
    float StrandStrength;
    float StrandCenterThickness;
    float StrandThickness;
    float CellScale;
    float CellStrength;
    float SeasonOffset;

    uint TopConvex : 1;
    uint IsNeedle  : 1;
};

struct FruitParameters {
    float  Chance;
    float  DownForce;
    float  Size;
    float4 Shape;
    float4 Color;
};

struct TreeParameters {
    int    Levels;
    float4 nBaseSize;
    float  AttractionUp;
    float  Flare;
    int    Lobes;
    float  LobeDepth;
    float  Scale;
    float  ScaleV;
    float  Ratio;
    float  RatioPower;
    int4   nShape;
    int4   nBaseSplits;
    float4 nSegSplits;
    float4 nSegSplitBaseOffset;
    float4 nSplitAngle;
    float4 nSplitAngleV;
    int4   nBranches;
    float4 nLength;
    float4 nLengthV;
    float4 nCurve;
    float4 nCurveV;
    float4 nCurveBack;
    float4 nRotate;
    float4 nRotateV;
    float4 nDownAngle;
    float4 nDownAngleV;
    int4   nCurveRes;
    float4 nTaper;

    LeafParameters  Leaf;
    LeafParameters  Blossom;
    FruitParameters Fruit;

    // Flag-type bitfield
    uint StemBirchTexture : 1;

    float4 StemSmallColor;
    float4 StemBigColor;
    float  StemBumpStrength;
    float  StemBumpGapSize;
    float  StemBumpVoronoiWeight;
    float  StemLichenFrequency;
    float  StemLichenSize;
};

#define TREE_TYPE_APPLE 0
#define TREE_TYPE_SASSAFRAS 1
#define TREE_TYPE_PALM 2
#define TREE_TYPE_TAMARACK 3
#define TREE_TYPE_COUNT 4

TreeParameters GetTreeParameters() {
    TreeParameters parameters;

    const uint treeType = LoadPersistentConfigUint(PersistentConfig::TREE_TYPE);

    if (treeType == TREE_TYPE_APPLE) {
        parameters.Levels                    = 3;
        parameters.nBaseSize                 = float4(0.15, 0.217, 0, 0.05);
        parameters.AttractionUp              = 2.f;
        parameters.Flare                     = 0.9;
        parameters.Lobes                     = 0;
        parameters.LobeDepth                 = 0;
        parameters.Scale                     = 4.5;
        parameters.ScaleV                    = 1;
        parameters.Ratio                     = 0.02;
        parameters.RatioPower                = 1.5;
        parameters.nShape                    = int4(2, 2, 4, 0);
        parameters.nBaseSplits               = int4(0, 0, 0, 0);
        parameters.nSegSplits                = float4(0, 0.474, 0, 0);
        parameters.nSegSplitBaseOffset       = float4(0, 0, 0, 0);
        parameters.nSplitAngle               = float4(0, 20, 0, 0);
        parameters.nSplitAngleV              = float4(0, 10, 0, 0);
        parameters.nBranches                 = int4(0, 28, 100, 10);
        parameters.nLength                   = float4(1, 0.5, 0.4, 0);
        parameters.nLengthV                  = float4(0, 0, 0.1, 0);
        parameters.nCurve                    = float4(0, -20, 0, 0);
        parameters.nCurveV                   = float4(30, 140, 100, 0);
        parameters.nCurveBack                = float4(0, 0, 0, 0);
        parameters.nRotate                   = float4(0, 140, 140, 77);
        parameters.nRotateV                  = float4(0, 0, 0, 0);
        parameters.nDownAngle                = float4(0, 60, 60, 45);
        parameters.nDownAngleV               = float4(0, -30, 20, 30);
        parameters.nCurveRes                 = int4(5, 10, 5, 1);
        parameters.nTaper                    = float4(1, 1, 1, 1);

        parameters.Leaf.Count                = 13;
        parameters.Leaf.ScaleShape           = 3;
        parameters.Leaf.Scale                = 0.085;
        parameters.Leaf.ScaleX               = 0.465;
        parameters.Leaf.StemLen              = 0.01;
        parameters.Leaf.BotAngle             = -85;
        parameters.Leaf.MidAngle             = 0;
        parameters.Leaf.TopAngle             = 45;
        parameters.Leaf.SideOffset           = 0.45;
        parameters.Leaf.Lobes                = 1;
        parameters.Leaf.LobeAngle            = 0;
        parameters.Leaf.LobeFalloff          = 0;
        parameters.Leaf.Color                = float4(0, 0.225, 0, 1);
        parameters.Leaf.Translucency         = 0.7;
        parameters.Leaf.StrandAngle          = 110;
        parameters.Leaf.StrandCurve          = 1.4;
        parameters.Leaf.StrandStep           = 0.2;
        parameters.Leaf.StrandOffset         = 0.6;
        parameters.Leaf.StrandStrength       = 0.5;
        parameters.Leaf.StrandCenterThickness = 2;
        parameters.Leaf.StrandThickness      = 0.6;
        parameters.Leaf.CellScale            = 256;
        parameters.Leaf.CellStrength         = 0.3;
        parameters.Leaf.SeasonOffset         = -0.186;
        parameters.Leaf.TopConvex            = 0;
        parameters.Leaf.IsNeedle             = 0;

        parameters.Blossom.Count             = 10;
        parameters.Blossom.ScaleShape        = 3;
        parameters.Blossom.Scale             = 0.0495;
        parameters.Blossom.ScaleX            = 0.612;
        parameters.Blossom.StemLen           = 0.01;
        parameters.Blossom.BotAngle          = -85;
        parameters.Blossom.MidAngle          = 0;
        parameters.Blossom.TopAngle          = 45;
        parameters.Blossom.SideOffset        = 0.45;
        parameters.Blossom.Lobes             = 4;
        parameters.Blossom.LobeAngle         = 39.556;
        parameters.Blossom.LobeFalloff       = 0.02;
        parameters.Blossom.Color             = float4(0.48, 0.35, 0.48, 1);
        parameters.Blossom.Translucency      = 0.7;
        parameters.Blossom.StrandAngle       = 110;
        parameters.Blossom.StrandCurve       = 1.4;
        parameters.Blossom.StrandStep        = 0.1;
        parameters.Blossom.StrandOffset      = 0.6;
        parameters.Blossom.StrandStrength    = 0.5;
        parameters.Blossom.StrandCenterThickness = 2;
        parameters.Blossom.StrandThickness   = 0.6;
        parameters.Blossom.CellScale         = 256;
        parameters.Blossom.CellStrength      = 0.3;
        parameters.Blossom.SeasonOffset      = 0;
        parameters.Blossom.TopConvex         = 0;
        parameters.Blossom.IsNeedle          = 0;

        parameters.Fruit.Chance              = 0.03;
        parameters.Fruit.DownForce           = 1;
        parameters.Fruit.Size                = 0.06;
        parameters.Fruit.Shape               = float4(0.685, -0.4, 0.74, 1.1);
        parameters.Fruit.Color               = float4(0.29302323, 0.1042142, 0.040886965, 1);

        parameters.StemBirchTexture          = 0;
        parameters.StemSmallColor            = float4(0.175, 0.25, 0.15, 1);
        parameters.StemBigColor              = float4(0.24, 0.2, 0.17, 1);
        parameters.StemBumpStrength          = 1;
        parameters.StemBumpGapSize           = 0.14;
        parameters.StemBumpVoronoiWeight     = 0.5;
        parameters.StemLichenFrequency       = 8;
        parameters.StemLichenSize            = 0.7;
    } else if (treeType == TREE_TYPE_SASSAFRAS) {
        parameters.Levels                    = 4;
        parameters.nBaseSize                 = float4(0.2, 0.05, 0.05, 0.05);
        parameters.AttractionUp              = 0.5;
        parameters.Flare                     = 0.5;
        parameters.Lobes                     = 3;
        parameters.LobeDepth                 = 0.05;
        parameters.Scale                     = 11.4625;
        parameters.ScaleV                    = 3.5;
        parameters.Ratio                     = 0.02;
        parameters.RatioPower                = 1.3;
        parameters.nShape                    = int4(2, 0, 0, 4);
        parameters.nBaseSplits               = int4(0, 0, 0, 0);
        parameters.nSegSplits                = float4(0, 0, 0, 0);
        parameters.nSegSplitBaseOffset       = float4(0, 0, 0, 0);
        parameters.nSplitAngle               = float4(20, 0, 0, 0);
        parameters.nSplitAngleV              = float4(5, 0, 0, 0);
        parameters.nBranches                 = int4(0, 15, 20, 30);
        parameters.nLength                   = float4(1, 0.4, 0.7, 0.4);
        parameters.nLengthV                  = float4(0, 0, 0, 0);
        parameters.nCurve                    = float4(0, -60, -40, 0);
        parameters.nCurveV                   = float4(10, 200, 300, 200);
        parameters.nCurveBack                = float4(0, 30, 0, 0);
        parameters.nRotate                   = float4(0, 140, 140, 140);
        parameters.nRotateV                  = float4(0, 0, 0, 0);
        parameters.nDownAngle                = float4(0, 90, 50, 45);
        parameters.nDownAngleV               = float4(0, -10, 10, 10);
        parameters.nCurveRes                 = int4(16, 15, 8, 3);
        parameters.nTaper                    = float4(1.05, 1, 1, 1);

        parameters.Leaf.Count                = 12;
        parameters.Leaf.ScaleShape           = 3;
        parameters.Leaf.Scale                = 0.125;
        parameters.Leaf.ScaleX               = 0.286;
        parameters.Leaf.StemLen              = 0.01;
        parameters.Leaf.BotAngle             = -85.889;
        parameters.Leaf.MidAngle             = -40;
        parameters.Leaf.TopAngle             = 90;
        parameters.Leaf.SideOffset           = 0.63;
        parameters.Leaf.Lobes                = 3;
        parameters.Leaf.LobeAngle            = 32.285;
        parameters.Leaf.LobeFalloff          = 0.123;
        parameters.Leaf.Color                = float4(0, 0.25, 0, 1);
        parameters.Leaf.Translucency         = 0.7;
        parameters.Leaf.StrandAngle          = 110;
        parameters.Leaf.StrandCurve          = 1.4;
        parameters.Leaf.StrandStep           = 0.1;
        parameters.Leaf.StrandOffset         = 0.6;
        parameters.Leaf.StrandStrength       = 0.5;
        parameters.Leaf.StrandCenterThickness = 2;
        parameters.Leaf.StrandThickness      = 0.6;
        parameters.Leaf.CellScale            = 256;
        parameters.Leaf.CellStrength         = 0.3;
        parameters.Leaf.SeasonOffset         = 0.347;
        parameters.Leaf.TopConvex            = 1;
        parameters.Leaf.IsNeedle             = 0;

        parameters.Blossom.Count             = 8;
        parameters.Blossom.ScaleShape        = 3;
        parameters.Blossom.Scale             = 0.0505;
        parameters.Blossom.ScaleX            = 0.303;
        parameters.Blossom.StemLen           = 0.01;
        parameters.Blossom.BotAngle          = -85;
        parameters.Blossom.MidAngle          = 0;
        parameters.Blossom.TopAngle          = 90;
        parameters.Blossom.SideOffset        = 0.45;
        parameters.Blossom.Lobes             = 5;
        parameters.Blossom.LobeAngle         = 40;
        parameters.Blossom.LobeFalloff       = 0;
        parameters.Blossom.Color             = float4(0.8184931, 0.5902166, 0.005606096, 1);
        parameters.Blossom.Translucency      = 0.7;
        parameters.Blossom.StrandAngle       = 110;
        parameters.Blossom.StrandCurve       = 1.4;
        parameters.Blossom.StrandStep        = 0.1;
        parameters.Blossom.StrandOffset      = 0.6;
        parameters.Blossom.StrandStrength    = 0.5;
        parameters.Blossom.StrandCenterThickness = 2;
        parameters.Blossom.StrandThickness   = 0.6;
        parameters.Blossom.CellScale         = 256;
        parameters.Blossom.CellStrength      = 0.3;
        parameters.Blossom.SeasonOffset      = 0;
        parameters.Blossom.TopConvex         = 1;
        parameters.Blossom.IsNeedle          = 0;

        parameters.Fruit.Chance              = 0.2;
        parameters.Fruit.DownForce           = 0.5;
        parameters.Fruit.Size                = 0.025;
        parameters.Fruit.Shape               = float4(0.649, 0, 0.5, 1);
        parameters.Fruit.Color               = float4(0.013326132, 0.014881321, 0.037209332, 1);

        parameters.StemBirchTexture          = 0;
        parameters.StemSmallColor            = float4(0.175, 0.25, 0.15, 1);
        parameters.StemBigColor              = float4(0.24, 0.2, 0.17, 1);
        parameters.StemBumpStrength          = 1;
        parameters.StemBumpGapSize           = 0.14;
        parameters.StemBumpVoronoiWeight     = 0.5;
        parameters.StemLichenFrequency       = 8;
        parameters.StemLichenSize            = 0.7;
    } else if (treeType == TREE_TYPE_PALM) {
        parameters.Levels                    = 2;
        parameters.nBaseSize                 = float4(0.95, 0.05, 0.05, 0.05);
        parameters.AttractionUp              = 0;
        parameters.Flare                     = 0;
        parameters.Lobes                     = 0;
        parameters.LobeDepth                 = 0;
        parameters.Scale                     = 6;
        parameters.ScaleV                    = 1.5;
        parameters.Ratio                     = 0.015;
        parameters.RatioPower                = 2;
        parameters.nShape                    = int4(4, 4, 0, 0);
        parameters.nBaseSplits               = int4(0, 0, 0, 0);
        parameters.nSegSplits                = float4(0, 0, 0, 0);
        parameters.nSegSplitBaseOffset       = float4(0, 0, 0, 0);
        parameters.nSplitAngle               = float4(0, 0, 0, 0);
        parameters.nSplitAngleV              = float4(0, 0, 0, 0);
        parameters.nBranches                 = int4(0, 33, 0, 0);
        parameters.nLength                   = float4(1, 0.4, 0, 0);
        parameters.nLengthV                  = float4(0, 0.05, 0, 0);
        parameters.nCurve                    = float4(20, 50, 0, 0);
        parameters.nCurveV                   = float4(10, 20, 0, 0);
        parameters.nCurveBack                = float4(-5, 0, 0, 0);
        parameters.nRotate                   = float4(0, 120, -120, 0);
        parameters.nRotateV                  = float4(0, 60, 20, 0);
        parameters.nDownAngle                = float4(0, 70, 50, 0);
        parameters.nDownAngleV               = float4(0, -80, -50, 0);
        parameters.nCurveRes                 = int4(12, 9, 1, 1);
        parameters.nTaper                    = float4(2.1, 1, 0, 0);

        parameters.Leaf.Count                = 250;
        parameters.Leaf.ScaleShape           = 3;
        parameters.Leaf.Scale                = 0.3;
        parameters.Leaf.ScaleX               = 0.06;
        parameters.Leaf.StemLen              = 0.01;
        parameters.Leaf.BotAngle             = -85;
        parameters.Leaf.MidAngle             = 0;
        parameters.Leaf.TopAngle             = 45;
        parameters.Leaf.SideOffset           = 0.45;
        parameters.Leaf.Lobes                = 1;
        parameters.Leaf.LobeAngle            = 0;
        parameters.Leaf.LobeFalloff          = 0;
        parameters.Leaf.Color                = float4(0.020941045, 0.10232556, 0.020941045, 1);
        parameters.Leaf.Translucency         = 0.7;
        parameters.Leaf.StrandAngle          = 110;
        parameters.Leaf.StrandCurve          = 1.4;
        parameters.Leaf.StrandStep           = 2;
        parameters.Leaf.StrandOffset         = 0.6;
        parameters.Leaf.StrandStrength       = 4;
        parameters.Leaf.StrandCenterThickness = 0;
        parameters.Leaf.StrandThickness      = 0;
        parameters.Leaf.CellScale            = 256;
        parameters.Leaf.CellStrength         = 0.3;
        parameters.Leaf.SeasonOffset         = -0.75;
        parameters.Leaf.TopConvex            = 0;
        parameters.Leaf.IsNeedle             = 0;

        parameters.Blossom.Count             = 0;
        parameters.Blossom.ScaleShape        = 3;
        parameters.Blossom.Scale             = 0.1;
        parameters.Blossom.ScaleX            = 0.5;
        parameters.Blossom.StemLen           = 0.01;
        parameters.Blossom.BotAngle          = -85;
        parameters.Blossom.MidAngle          = 0;
        parameters.Blossom.TopAngle          = 45;
        parameters.Blossom.SideOffset        = 0.45;
        parameters.Blossom.Lobes             = 5;
        parameters.Blossom.LobeAngle         = 0;
        parameters.Blossom.LobeFalloff       = 0;
        parameters.Blossom.Color             = float4(0, 0.125, 0, 1);
        parameters.Blossom.Translucency      = 0.7;
        parameters.Blossom.StrandAngle       = 110;
        parameters.Blossom.StrandCurve       = 1.4;
        parameters.Blossom.StrandStep        = 0.1;
        parameters.Blossom.StrandOffset      = 0.6;
        parameters.Blossom.StrandStrength    = 0.5;
        parameters.Blossom.StrandCenterThickness = 2;
        parameters.Blossom.StrandThickness   = 0.6;
        parameters.Blossom.CellScale         = 256;
        parameters.Blossom.CellStrength      = 0.3;
        parameters.Blossom.SeasonOffset      = 0;
        parameters.Blossom.TopConvex         = 0;
        parameters.Blossom.IsNeedle          = 0;

        parameters.Fruit.Chance              = 0;
        parameters.Fruit.DownForce           = 0;
        parameters.Fruit.Size                = 0.1;
        parameters.Fruit.Shape               = float4(0.5, 0.333, 0.5, 0.666);
        parameters.Fruit.Color               = float4(0.25, 0, 0, 1);

        parameters.StemBirchTexture          = 0;
        parameters.StemSmallColor            = float4(0.175, 0.25, 0.15, 1);
        parameters.StemBigColor              = float4(0.24, 0.2, 0.17, 1);
        parameters.StemBumpStrength          = 1;
        parameters.StemBumpGapSize           = 0.14;
        parameters.StemBumpVoronoiWeight     = 0.5;
        parameters.StemLichenFrequency       = 8;
        parameters.StemLichenSize            = 0.7;
    } else if (treeType == TREE_TYPE_TAMARACK) {
        parameters.Levels                    = 3;
        parameters.nBaseSize                 = float4(0.1, 0.05, 0.05, 0.05);
        parameters.AttractionUp              = 0.5;
        parameters.Flare                     = 0.4;
        parameters.Lobes                     = 0;
        parameters.LobeDepth                 = 0;
        parameters.Scale                     = 10.925;
        parameters.ScaleV                    = 1.5;
        parameters.Ratio                     = 0.015;
        parameters.RatioPower                = 1.3;
        parameters.nShape                    = int4(0, 0, 4, 0);
        parameters.nBaseSplits               = int4(0, 0, 0, 0);
        parameters.nSegSplits                = float4(0, 0, 0, 0);
        parameters.nSegSplitBaseOffset       = float4(0, 0, 0, 0);
        parameters.nSplitAngle               = float4(0, 0, 0, 0);
        parameters.nSplitAngleV              = float4(0, 0, 0, 0);
        parameters.nBranches                 = int4(0, 75, 50, 0);
        parameters.nLength                   = float4(1, 0.4, 0.2, 0);
        parameters.nLengthV                  = float4(0, 0, 0, 0);
        parameters.nCurve                    = float4(0, -30, 0, 0);
        parameters.nCurveV                   = float4(0, 120, 180, 0);
        parameters.nCurveBack                = float4(0, 0, 0, 0);
        parameters.nRotate                   = float4(0, 140, 140, 140);
        parameters.nRotateV                  = float4(0, 0, 0, 0);
        parameters.nDownAngle                = float4(0, 55, 45, 45);
        parameters.nDownAngleV               = float4(0, -45, 10, 10);
        parameters.nCurveRes                 = int4(8, 8, 8, 1);
        parameters.nTaper                    = float4(0.9, 1, 1, 0);

        parameters.Leaf.Count                = 50;
        parameters.Leaf.ScaleShape           = 3;
        parameters.Leaf.Scale                = 0.1;
        parameters.Leaf.ScaleX               = 0.35;
        parameters.Leaf.StemLen              = 0;
        parameters.Leaf.BotAngle             = -85;
        parameters.Leaf.MidAngle             = 0;
        parameters.Leaf.TopAngle             = 45;
        parameters.Leaf.SideOffset           = 0.45;
        parameters.Leaf.Lobes                = 1;
        parameters.Leaf.LobeAngle            = 0;
        parameters.Leaf.LobeFalloff          = 0;
        parameters.Leaf.Color                = float4(0, 0.2, 0, 1);
        parameters.Leaf.Translucency         = 0.7;
        parameters.Leaf.StrandAngle          = 110;
        parameters.Leaf.StrandCurve          = 1.4;
        parameters.Leaf.StrandStep           = 0.1;
        parameters.Leaf.StrandOffset         = 0.6;
        parameters.Leaf.StrandStrength       = 0.5;
        parameters.Leaf.StrandCenterThickness = 2;
        parameters.Leaf.StrandThickness      = 0.6;
        parameters.Leaf.CellScale            = 256;
        parameters.Leaf.CellStrength         = 0.3;
        parameters.Leaf.SeasonOffset         = -1;
        parameters.Leaf.TopConvex            = 0;
        parameters.Leaf.IsNeedle             = 1;

        parameters.Blossom.Count             = 0;
        parameters.Blossom.ScaleShape        = 3;
        parameters.Blossom.Scale             = 0.1;
        parameters.Blossom.ScaleX            = 0.5;
        parameters.Blossom.StemLen           = 0;
        parameters.Blossom.BotAngle          = -85;
        parameters.Blossom.MidAngle          = 0;
        parameters.Blossom.TopAngle          = 45;
        parameters.Blossom.SideOffset        = 0.45;
        parameters.Blossom.Lobes             = 5;
        parameters.Blossom.LobeAngle         = 0;
        parameters.Blossom.LobeFalloff       = 0;
        parameters.Blossom.Color             = float4(0, 0.125, 0, 1);
        parameters.Blossom.Translucency      = 0.7;
        parameters.Blossom.StrandAngle       = 110;
        parameters.Blossom.StrandCurve       = 1.4;
        parameters.Blossom.StrandStep        = 0.1;
        parameters.Blossom.StrandOffset      = 0.6;
        parameters.Blossom.StrandStrength    = 0.5;
        parameters.Blossom.StrandCenterThickness = 2;
        parameters.Blossom.StrandThickness   = 0.6;
        parameters.Blossom.CellScale         = 256;
        parameters.Blossom.CellStrength      = 0.3;
        parameters.Blossom.SeasonOffset      = 0;
        parameters.Blossom.TopConvex         = 0;
        parameters.Blossom.IsNeedle          = 0;

        parameters.Fruit.Chance              = 0;
        parameters.Fruit.DownForce           = 0;
        parameters.Fruit.Size                = 0.1;
        parameters.Fruit.Shape               = float4(0.5, 0.333, 0.5, 0.666);
        parameters.Fruit.Color               = float4(0.25, 0, 0, 1);

        parameters.StemBirchTexture          = 0;
        parameters.StemSmallColor            = float4(0.175, 0.25, 0.15, 1);
        parameters.StemBigColor              = float4(0.24, 0.2, 0.17, 1);
        parameters.StemBumpStrength          = 1;
        parameters.StemBumpGapSize           = 0.14;
        parameters.StemBumpVoronoiWeight     = 0.5;
        parameters.StemLichenFrequency       = 8;
        parameters.StemLichenSize            = 0.7;
    } else {
        parameters.Levels              = 3;
        parameters.nBaseSize           = float4(0.25, 0.05, 0.05, 0.05);
        parameters.AttractionUp        = 0.0;
        parameters.Flare               = 0.5;
        parameters.Lobes               = 0;
        parameters.LobeDepth           = 0.0;
        parameters.Scale               = 10.0;
        parameters.ScaleV              = 0.0;
        parameters.Ratio               = 0.05;
        parameters.RatioPower          = 1.0;
        parameters.nShape              = int4(0, 0, 0, 0);
        parameters.nBaseSplits         = int4(0, 0, 0, 0);
        parameters.nSegSplits          = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nSegSplitBaseOffset = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nSplitAngle         = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nSplitAngleV        = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nBranches           = int4(1, 10, 5, 0);
        parameters.nLength             = float4(1.0, 0.5, 0.5, 0.0);
        parameters.nLengthV            = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nCurve              = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nCurveV             = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nCurveBack          = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nRotate             = float4(0.0, 120.0, 120.0, 120.0);
        parameters.nRotateV            = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nDownAngle          = float4(0.0, 30.0, 30.0, 30.0);
        parameters.nDownAngleV         = float4(0.0, 0.0, 0.0, 0.0);
        parameters.nCurveRes           = int4(3, 3, 1, 0);
        parameters.nTaper              = float4(1.0, 1.0, 1.0, 0.0);

        parameters.Leaf.Count                 = 100;
        parameters.Leaf.ScaleShape            = 3;
        parameters.Leaf.Scale                 = 0.2;
        parameters.Leaf.ScaleX                = 0.5;
        parameters.Leaf.StemLen               = 0.5;
        parameters.Leaf.BotAngle              = -85.0;
        parameters.Leaf.MidAngle              = 0.0;
        parameters.Leaf.TopAngle              = 45.0;
        parameters.Leaf.SideOffset            = 0.45;
        parameters.Leaf.Lobes                 = 1;
        parameters.Leaf.LobeAngle             = 0.0;
        parameters.Leaf.LobeFalloff           = 0.0;
        parameters.Leaf.Color                 = float4(0.0, 0.125, 0.0, 1.0);
        parameters.Leaf.Translucency          = 0.7;
        parameters.Leaf.StrandAngle           = 110;
        parameters.Leaf.StrandCurve           = 1.4;
        parameters.Leaf.StrandStep            = 0.1;
        parameters.Leaf.StrandOffset          = 0.6;
        parameters.Leaf.StrandStrength        = 0.5;
        parameters.Leaf.StrandCenterThickness = 2;
        parameters.Leaf.StrandThickness       = 0.6;
        parameters.Leaf.CellScale             = 256;
        parameters.Leaf.CellStrength          = 0.3;
        parameters.Leaf.SeasonOffset          = 0;
        parameters.Leaf.TopConvex             = false;
        parameters.Leaf.IsNeedle              = false;

        parameters.Blossom.Count                 = 0;
        parameters.Blossom.ScaleShape            = 3;
        parameters.Blossom.Scale                 = 0.2;
        parameters.Blossom.ScaleX                = 0.5;
        parameters.Blossom.StemLen               = 0.5;
        parameters.Blossom.BotAngle              = -85.0;
        parameters.Blossom.MidAngle              = 0.0;
        parameters.Blossom.TopAngle              = 45.0;
        parameters.Blossom.SideOffset            = 0.45;
        parameters.Blossom.Lobes                 = 5;
        parameters.Blossom.LobeAngle             = 0.0;
        parameters.Blossom.LobeFalloff           = 0.0;
        parameters.Blossom.Color                 = float4(0.0, 0.125, 0.0, 1.0);
        parameters.Blossom.Translucency          = 0.7;
        parameters.Blossom.StrandAngle           = 110;
        parameters.Blossom.StrandCurve           = 1.4;
        parameters.Blossom.StrandStep            = 0.1;
        parameters.Blossom.StrandOffset          = 0.6;
        parameters.Blossom.StrandStrength        = 0.5;
        parameters.Blossom.StrandCenterThickness = 2;
        parameters.Blossom.StrandThickness       = 0.6;
        parameters.Blossom.CellScale             = 256;
        parameters.Blossom.CellStrength          = 0.3;
        parameters.Blossom.SeasonOffset          = 0;
        parameters.Blossom.TopConvex             = false;
        parameters.Blossom.IsNeedle              = false;
                
        parameters.Fruit.Chance         = 0;
        parameters.Fruit.DownForce      = 1;
        parameters.Fruit.Size           = 0.1;
        parameters.Fruit.Shape          = float4(.5, .333, .5, .666);
        parameters.Fruit.Color          = float4(.25, 0, 0, 1);
                
        parameters.StemBirchTexture          = false;
        parameters.StemSmallColor            = float4(.175, .25, .15, 1.0);
        parameters.StemBigColor              = float4(.24, .20, .17, 1.0);
        parameters.StemBumpStrength          = 1.f;
        parameters.StemBumpGapSize           = .14f;
        parameters.StemBumpVoronoiWeight     = 0.5f;
        parameters.StemLichenFrequency       = 8;
        parameters.StemLichenSize            = 0.7f;
    }

    return parameters;
}

LeafParameters GetLeafParameters(in const TreeParameters treeParameters, in const bool isBlossom) {
    if (isBlossom) {
        return treeParameters.Blossom;
    } else {
        return treeParameters.Leaf;
    }
}