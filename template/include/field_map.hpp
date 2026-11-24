#pragma once
#include "Reaper-Template/field_map_core.hpp"

// FIELD MAP FOR LOCALIZATION
// List the field elements that can block a distance sensor's view of the wall: goals, loaders, barriers.
// Localization then ignores any sensor whose beam would pass near one of them. Edit the SETTINGS section for
// your game, rebuild, upload. Guide and measuring tips: docs/localization.md ("Field map").
//
// Coordinates: inches from the field center, +X right and +Y forward as seen from your starting side (the
// same frame as chassis.odom_xyt_set()). Take positions from your game manual's field drawings, then check
// them with a tape measure on a real field.
namespace field_map {

inline localization::FieldMap obstacles() {
  localization::FieldMap map;

  // ========================== SETTINGS ==========================
  // Clearance added around every obstacle (inches): covers position error and the beam's width.
  map.margin = 2.0;

  // Round things: {x, y, radius}
  map.circles = {
      // {0.0, 0.0, 5.0},       // example: a round goal in the middle of the field
      // {21.0, 47.5, 4.0},     // example: a goal leg
  };

  // Straight things: {x1, y1, x2, y2}
  map.segments = {
      // {-24.0, 0.0, 24.0, 0.0},  // example: a barrier across the middle
  };
  // ======================== END SETTINGS ========================

  return map;
}

}  // namespace field_map
