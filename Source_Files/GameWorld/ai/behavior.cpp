#include <string.h>
#include <limits.h>

#include "cseries.h"
#include "map.h"
#include "render.h"
#include "interface.h"
#include "FilmProfile.h"
#include "flood_map.h"
#include "effects.h"

#include "behavior.hpp"

#include "monsters.h"
#include "projectiles.h"
#include "player.h"
#include "platforms.h"
#include "scenery.h"
#include "SoundManager.h"
#include "fades.h"
#include "items.h"
#include "media.h"

monsterBehavior* monsterBehavior::Get(const char* className, const char* behaviorName)
{
  return nullptr;
}
