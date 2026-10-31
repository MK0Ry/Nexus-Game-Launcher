#include "nexus/model/game.h"

#include <QFileInfo>

namespace nexus {

bool Game::executableExists() const
{
    const QFileInfo info(executablePath);
    return info.exists() && info.isFile();
}

} // namespace nexus
