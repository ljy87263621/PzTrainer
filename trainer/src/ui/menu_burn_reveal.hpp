#pragma once

namespace pztrainer::ui {

struct TrainerMenuLayout;

void BeginMenuBurnReveal();
bool MenuBurnRevealComplete();
void DrawMenuBurnReveal(const TrainerMenuLayout& layout);

}  // namespace pztrainer::ui
