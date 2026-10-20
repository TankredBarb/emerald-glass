// SPDX-License-Identifier: LGPL-3.0-or-later
#include "blurconfig.h"
#include "settings.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
    QCoreApplication app(argc, argv);
    const auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    KConfigGroup foreign(config, QStringLiteral("Effect-blurplus"));
    foreign.writeEntry("BlurMatching", true);
    foreign.writeEntry("WindowClasses", "org.kde.dolphin");
    foreign.writeEntry("BlurFinetune", 10);
    foreign.sync();
    KWin::BlurConfig::instance(config);
    KWin::BlurSettings settings;
    settings.read();
    if (settings.forceBlur.windowClassMatchingMode != KWin::WindowClassMatchingMode::Blacklist ||
        !settings.forceBlur.windowClasses.isEmpty() || qAbs(settings.general.blurRadius - .89f) > .0001f) {
        std::cerr << "BlurPlus configuration leaked into Emerald Glass\n";
        return 1;
    }
    KConfigGroup own(config, QStringLiteral("Effect-emeraldglass"));
    own.writeEntry("BlurFinetune", 5);
    own.sync();
    settings.read();
    if (qAbs(settings.general.blurRadius - 1.15f) > .0001f) return 1;
    std::cout << "PASS: production effect settings are isolated from BlurPlus\n";
}
