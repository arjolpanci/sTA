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
// Street dressing placed along the road network by World::placeProps().
// height is the real-world size the normalized model is scaled back up to,
// radius its footprint, and weight how many of a hundred kerbside slots this
// prop wins - the rest stay empty. Weight 0 means the prop is not part of that
// draw at all: the two street lamps are placed on a regular cadence instead,
// because a street reads as a street from evenly spaced lamps and as a junk
// yard from randomly spaced ones. alongRoad orients a prop with the kerb rather
// than facing across it, which is what a bench or a parked car wants.
struct PropModel { const char* name; float height, radius; int weight; bool solid, alongRoad; };
inline constexpr std::array<PropModel,8> PropModels{{
    { "street_lamp",   5.0f, 0.30f, 0, true,  false },
    { "fire_hydrant",  0.9f, 0.30f, 5, true,  false },
    { "trash_can",     1.0f, 0.35f, 8, true,  false },
    { "street_bench",  0.9f, 0.80f, 5, true,  true  },
    { "utility_box",   1.3f, 0.50f, 4, true,  false },
    { "road_barrier",  1.0f, 1.10f, 3, true,  true  },
    { "covered_car",   1.5f, 2.10f, 3, true,  true  },
    { "old_tyre",      0.7f, 0.40f, 3, false, true  },
}};
// Manhole covers go on the road itself rather than the kerb, so they are placed
// on their own and are never solid.
inline constexpr const char* ManholeModel = "manhole_cover";
inline constexpr float ManholeHeight = 0.12f;
// The lamp is index 0 above, placed every LampSpacing metres, alternating sides.
inline constexpr float LampSpacing = 22.0f;

inline constexpr std::array<const char*,12> TreeModels{{
    "tree_jacaranda", "tree_island_a", "tree_island_b", "tree_island_c", "tree_small", "tree_jacaranda_airy",
    "tree_island_dense", "tree_island_wide", "tree_small_open", "tree_island_slim", "tree_quiver_a", "tree_quiver_b"
}};
