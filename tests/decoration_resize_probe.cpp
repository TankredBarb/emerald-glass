// SPDX-License-Identifier: LGPL-3.0-or-later
#include "../native/material.h"
#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/Private/DecoratedWindowPrivate>
#include <KDecoration3/Private/DecorationBridge>
#include <KDecoration3/Private/DecorationSettingsPrivate>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QApplication>
#include <QTemporaryDir>
#include <iostream>
#include <QElapsedTimer>
#include <algorithm>
#include <vector>
using namespace KDecoration3;
class Client : public DecoratedWindowPrivateV4 {
public:
 Client(DecoratedWindow *w,Decoration *d):DecoratedWindowPrivateV4(w,d){}
#define BOOL(name,value) bool name() const override { return value; }
 bool active=true;
 bool isActive() const override { return active; }
 BOOL(isOnAllDesktops,false) BOOL(isShaded,false) BOOL(isMaximized,false)
 BOOL(isMaximizedHorizontally,false) BOOL(isMaximizedVertically,false) BOOL(isKeepAbove,false) BOOL(isKeepBelow,false)
 BOOL(isCloseable,true) BOOL(isMaximizeable,true) BOOL(isMinimizeable,true) BOOL(providesContextHelp,false)
 BOOL(isModal,false) BOOL(isShadeable,true) BOOL(isMoveable,true) BOOL(isResizeable,true)
 BOOL(hasApplicationMenu,false) BOOL(isApplicationMenuActive,false) BOOL(isExcludedFromCapture,false)
#undef BOOL
 QString caption() const override { return QStringLiteral("scope test"); }
 QIcon icon() const override { return {}; }
 qreal clientWidth=1280, clientHeight=720;
 qreal width() const override { return clientWidth; } qreal height() const override { return clientHeight; }
 QSizeF size() const override { return {width(),height()}; }
 QPalette palette() const override { return {}; }
 Qt::Edges adjacentScreenEdges() const override { return {}; }
 qreal scale() const override { return 1; } qreal nextScale() const override { return 1; }
 QString windowClass() const override { return QStringLiteral("scope-test"); }
 QString applicationMenuServiceName() const override { return {}; }
 QString applicationMenuObjectPath() const override { return {}; }
#define NOOP(name) void name() override {}
 NOOP(requestHideToolTip) NOOP(requestClose) NOOP(requestMinimize) NOOP(requestContextHelp)
 NOOP(requestToggleOnAllDesktops) NOOP(requestToggleShade) NOOP(requestToggleKeepAbove) NOOP(requestToggleKeepBelow)
 NOOP(requestToggleExcludeFromCapture)
#undef NOOP
 void requestShowToolTip(const QString &) override {} void requestToggleMaximization(Qt::MouseButtons) override {}
 void requestShowWindowMenu(const QRect &) override {} void showApplicationMenu(int) override {}
 void requestShowApplicationMenu(const QRect &,int) override {} void popup(const Positioner &,QMenu *) override {}
};
class Settings : public DecorationSettingsPrivateV2 {
public:
 Settings(DecorationSettings *s):DecorationSettingsPrivateV2(s){}
 bool isOnAllDesktopsAvailable() const override { return true; }
 bool isAlphaChannelSupported() const override { return true; }
 bool isCloseOnDoubleClickOnMenu() const override { return false; }
 bool isAlwaysShowExcludeFromCapture() const override { return false; }
 QList<DecorationButtonType> decorationButtonsLeft() const override { return {DecorationButtonType::Menu,DecorationButtonType::KeepAbove,DecorationButtonType::OnAllDesktops}; }
 QList<DecorationButtonType> decorationButtonsRight() const override { return {DecorationButtonType::Minimize,DecorationButtonType::Maximize,DecorationButtonType::Close}; }
 BorderSize borderSize() const override { return BorderSize::Normal; }
};
class Bridge : public DecorationBridge {
public:
 Client *client=nullptr;
 std::unique_ptr<DecoratedWindowPrivate> createClient(DecoratedWindow *w,Decoration *d) override { auto result=std::make_unique<Client>(w,d); client=result.get(); return result; }
 std::unique_ptr<DecorationSettingsPrivate> settings(DecorationSettings *s) override { return std::make_unique<Settings>(s); }
};
void check(bool condition,const char *message) { if(!condition) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); } }
int main(int argc,char **argv) {
 QTemporaryDir config; qputenv("XDG_CONFIG_HOME",config.path().toUtf8()); QApplication app(argc,argv);
 check(argc==3,"usage: decoration-resize-probe PLUGIN {cpu|gpu|transformed}");
 const QString mode=QString::fromLocal8Bit(argv[2]);
 EmeraldGlass::MaterialConfig cfg; cfg.useSystemButtons=true; cfg.shaderEffectEnabled=mode!=QStringLiteral("cpu"); EmeraldGlass::saveMaterialConfig(cfg);
 Bridge bridge; auto settings=std::make_shared<DecorationSettings>(&bridge);
 QVariantMap args; args.insert("bridge",QVariant::fromValue<DecorationBridge *>(&bridge));
 auto result=KPluginFactory::instantiatePlugin<Decoration>(KPluginMetaData(QString::fromLocal8Bit(argv[1])),nullptr,{args});
 check(bool(result),"decoration loads"); std::unique_ptr<Decoration> d(result.plugin);
 d->setSettings(settings); d->create();
 QObject::connect(d.get(),&Decoration::nextStateChanged,d.get(),[&](const auto &state){d->apply(state);});
 check(d->init(),"decoration initializes"); d->setProperty("egShader",mode!=QStringLiteral("cpu")); d->setProperty("egTransformed",mode==QStringLiteral("transformed"));
 for(qreal dpr : {1.,2.}) {
  std::vector<double> samples;
  for(int iteration=-4;iteration<40;++iteration) {
   QElapsedTimer timer; timer.start();
   bridge.client->clientWidth=1280+(iteration+4)%32; bridge.client->clientHeight=720+(iteration+4)%16;
   d->window()->sizeChanged(bridge.client->size());
   const qreal w=d->size().width(),h=d->size().height(),top=d->borderTop(),bottom=d->borderBottom();
   const std::array<QRectF,4> parts={QRectF(0,0,w,top),QRectF(0,h-bottom,w,bottom),QRectF(0,top,d->borderLeft(),h-top-bottom),QRectF(w-d->borderRight(),top,d->borderRight(),h-top-bottom)};
   for(const auto &part:parts) {
    if(part.isEmpty()) continue;
    QImage image((part.size()*dpr).toSize(),QImage::Format_ARGB32_Premultiplied); image.setDevicePixelRatio(dpr); image.fill(Qt::transparent);
    QPainter painter(&image); painter.translate(-part.topLeft()); painter.setClipRect(part); d->paint(&painter,part); painter.end();
   }
   if(iteration>=0) samples.push_back(timer.nsecsElapsed()/1000000.);
  }
  std::sort(samples.begin(),samples.end());
  std::cout<<argv[1]<<" mode="<<mode.toStdString()<<" DPR="<<dpr<<" resize CPU ms p50="<<samples[samples.size()/2]<<" p95="<<samples[samples.size()*95/100]<<'\n';
 }
}
