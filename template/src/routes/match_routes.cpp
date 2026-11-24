#include "main.h"

ez::Auton match_01() {
  return {"Match 01", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_02() {
  return {"Match 02", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_03() {
  return {"Match 03", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_04() {
  return {"Match 04", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_05() {
  return {"Match 05", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_06() {
  return {"Match 06", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_07() {
  return {"Match 07", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_08() {
  return {"Match 08", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_09() {
  return {"Match 09", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_10() {
  return {"Match 10", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_11() {
  return {"Match 11", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_12() {
  return {"Match 12", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_13() {
  return {"Match 13", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_14() {
  return {"Match 14", "Empty route - add your movements here.", [] {
  }};
}

ez::Auton match_15() {
  return {"Match 15", "Empty route - add your movements here.", [] {
  }};
}

void register_routes() {
  ez::as::auton_selector.autons_add({
    match_01(), match_02(), match_03(), match_04(), match_05(), match_06(), match_07(), match_08(), match_09(), match_10(), match_11(), match_12(), match_13(), match_14(), match_15()
  });
  ez::as::auton_selector.skills_add({skills_01(), skills_02(), skills_03()});
}
