#pragma once
#include <array>

inline constexpr std::array<const char*,18> VehicleModels{{
    "sedan", "taxi", "van", "hatchback-sports", "sedan-sports", "suv", "suv-luxury", "police",
    "ambulance", "delivery", "delivery-flat", "truck", "truck-flat", "firetruck", "garbage-truck",
    "race", "race-future", "tractor-police"
}};
// Order is load-bearing: the baked island scene stores a variant index into
// this list, chosen by terrain (0-5 broadleaf, 6-9 highland, 10-11 coastal).
// See tools/import_polyhaven_trees.py for where these come from.
inline constexpr std::array<const char*,12> TreeModels{{
    "tree_jacaranda", "tree_island_a", "tree_island_b", "tree_island_c", "tree_small", "tree_jacaranda_airy",
    "tree_island_dense", "tree_island_wide", "tree_small_open", "tree_island_slim", "tree_quiver_a", "tree_quiver_b"
}};
