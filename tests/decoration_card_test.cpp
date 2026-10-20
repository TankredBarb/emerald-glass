// SPDX-License-Identifier: LGPL-3.0-or-later
#include "../native/material.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

int main(int argc, char **argv)
{
    QTemporaryDir config, plugins;
    bool capture=false;
    for(int i=2;i<argc;++i) if(QByteArray(argv[i])=="--capture") capture=true;
    if(!capture) qputenv("XDG_CONFIG_HOME",config.path().toUtf8());
    QApplication app(argc,argv);
    auto check=[](bool ok,const char *why) { if(!ok) {std::cerr<<why<<'\n';std::exit(1);} };
    check(argc>=2,"decoration plugin path required");
    QDir().mkpath(plugins.path()+"/org.kde.kdecoration3");
    check(QFile::link(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath(),
          plugins.path()+"/org.kde.kdecoration3/org.tankred.emeraldglass.so"),"isolated plugin lookup");
    QCoreApplication::addLibraryPath(plugins.path());
    if(!capture) {
        EmeraldGlass::MaterialConfig cfg;
        cfg.shaderEffectEnabled=true; cfg.frameOpacity=.32f; cfg.blurStrength=7;
        EmeraldGlass::saveMaterialConfig(cfg);
        KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("kwinrc")),QStringLiteral("Plugins"));
        group.writeEntry("emeraldglassEnabled",true);group.sync();
    }
    QQuickView view;
    QQmlComponent component(view.engine());
    component.setData(R"(
import QtQuick
import org.kde.kwin.private.kdecoration as Preview
Rectangle {
    width: 440; height: 260; color: "#e7e7e7"
    Preview.Bridge { id: bridge; plugin: "org.tankred.emeraldglass" }
    Preview.Settings { id: themeSettings; bridge: bridge.bridge }
    Preview.Decoration {
        objectName: "inactive"; x: 8; y: 8; width: 355; height: 185
        bridge: bridge.bridge; settings: themeSettings; windowColor: "#f3f4f5"
        Component.onCompleted: { client.active=false; client.caption="Inactive window" }
    }
    Preview.Decoration {
        objectName: "active"; x: 60; y: 54; width: 365; height: 190
        bridge: bridge.bridge; settings: themeSettings; windowColor: "#f3f4f5"
        Component.onCompleted: { client.active=true; client.caption="Active window" }
    }
})",QUrl(QStringLiteral("file:///emerald-decoration-card.qml")));
    if(component.isError()) std::cerr<<component.errorString().toStdString();
    check(component.isReady(),"KDE decoration card QML loads");
    auto *root=qobject_cast<QQuickItem *>(component.create());
    check(root,"KDE decoration card root");
    view.setContent(QUrl(),&component,root);view.show();
    const auto args=app.arguments();
    QTimer::singleShot(1200,&view,[&] {
        bool gpu=true;
        for(const char *name : {"active","inactive"}) {
            auto *item=root->findChild<QObject *>(QString::fromLatin1(name));check(item,"preview item exists");
            auto *decoration=item->property("decoration").value<QObject *>();check(decoration,"real KDE PreviewBridge created decoration");
            check(decoration->property("egPreview").toBool(),"real KDE bridge is detected without classifying live windows as previews");
            check(decoration->property("egPreviewMaterial").isValid(),"card requested its private material render");
            gpu &= decoration->property("egPreviewMaterial").toBool();
        }
        const auto image=view.grabWindow();check(!image.isNull(),"card composition renders");
        const auto dpr=image.devicePixelRatio();
        check(image.pixelColor(qRound(200*dpr),qRound(155*dpr))==QColor("#f3f4f5"),
              "card material never paints over the real preview client area");
        const int path=args.indexOf(QStringLiteral("--capture"));
        if(path>=0 && path+1<args.size()) check(image.save(args[path+1]),"card capture saved");
        if(args.contains(QStringLiteral("--require-gpu"))) check(gpu,"private material graphics context required");
        std::cout<<"PASS: real KDE PreviewBridge/card routing, active/inactive geometry and composition; GPU material="<<gpu<<'\n';
        app.exit(0);
    });
    return app.exec();
}
