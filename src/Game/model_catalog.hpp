#pragma once
#include <array>

inline constexpr std::array<const char*,18> VehicleModels{{
    "sedan", "taxi", "van", "hatchback-sports", "sedan-sports", "suv", "suv-luxury", "police",
    "ambulance", "delivery", "delivery-flat", "truck", "truck-flat", "firetruck", "garbage-truck",
    "race", "race-future", "tractor-police"
}};
inline constexpr std::array<const char*,12> TreeModels{{
    "tree_oak", "tree_default", "tree_detailed", "tree_fat", "tree_tall", "tree_thin",
    "tree_pineDefaultA", "tree_pineDefaultB", "tree_pineTallA", "tree_pineRoundC", "tree_palm", "tree_palmBend"
}};
