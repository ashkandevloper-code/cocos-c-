// World entry points (Dart: lib/worlds/*). Worlds not yet ported return nullptr and the Hub shows an
// honest "not ported yet" screen (tracked in docs/PORTING.md).
#include "cocos2d.h"

#include <string>

namespace nx {

cocos2d::Scene* createWorldScene(const std::string& id) {
    (void)id;
    return nullptr;
}

}  // namespace nx
