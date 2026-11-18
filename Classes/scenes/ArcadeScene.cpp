// Arcade world (Dart: lib/worlds/arcade/arcade_page.dart). Stage (a): list only.
#include "scenes/ShellScenes.h"
#include "ui/Widgets.h"
#include "core/AppSettings.h"

USING_NS_CC;

namespace nx {

cocos2d::Scene* createArcadeScene() {
    return NotPortedScene::create("آرکید نکسورا", "NEXORA ARCADE",
                                  tx("بازی‌ها در مرحله‌ی بعدی پورت اضافه می‌شوند.", "Games arrive in the next porting stage."));
}

}  // namespace nx
