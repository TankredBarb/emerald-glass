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
#include <KCModule>
#include <QApplication>
#include <QHoverEvent>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QVector4D>
#include <iostream>
using namespace KDecoration3;
class Client : public DecoratedWindowPrivateV4 {
public:
 Client(DecoratedWindow *w,Decoration *d):DecoratedWindowPrivateV4(w,d){}
#define BOOL(name,value) bool name() const override { return value; }
 bool active=true;
 bool isActive() const override { return active; }
 bool maximizedHorizontally=false, maximizedVertically=false;
 bool isMaximized() const override { return maximizedHorizontally && maximizedVertically; }
 bool isMaximizedHorizontally() const override { return maximizedHorizontally; }
 bool isMaximizedVertically() const override { return maximizedVertically; }
 BOOL(isOnAllDesktops,false) BOOL(isShaded,false)
 BOOL(isKeepAbove,false) BOOL(isKeepBelow,false)
 BOOL(isCloseable,true) BOOL(isMaximizeable,true) BOOL(isMinimizeable,true) BOOL(providesContextHelp,false)
 BOOL(isModal,false) BOOL(isShadeable,true) BOOL(isMoveable,true) BOOL(isResizeable,true)
 BOOL(hasApplicationMenu,false) BOOL(isApplicationMenuActive,false) BOOL(isExcludedFromCapture,false)
#undef BOOL
 QString caption() const override { return QStringLiteral("scope test"); }
 QIcon icon() const override { return {}; }
 qreal width() const override { return 500; } qreal height() const override { return 300; }
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
 QList<DecorationButtonType> decorationButtonsLeft() const override { return {}; }
 QList<DecorationButtonType> decorationButtonsRight() const override { return {DecorationButtonType::Close}; }
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
 check(argc==3,"decoration and KCM plugin paths"); EmeraldGlass::MaterialConfig cfg; cfg.blurStrength=0; EmeraldGlass::saveMaterialConfig(cfg);
 check(EmeraldGlass::loadMaterialConfig().blurStrength==0,"zero blur round trip");
 cfg.blurStrength=15; EmeraldGlass::saveMaterialConfig(cfg);
 check(EmeraldGlass::loadMaterialConfig().blurStrength==15,"maximum blur round trip");
 Bridge bridge; auto settings=std::make_shared<DecorationSettings>(&bridge);
 QVariantMap args; args.insert("bridge",QVariant::fromValue<DecorationBridge *>(&bridge));
 auto result=KPluginFactory::instantiatePlugin<Decoration>(KPluginMetaData(QString::fromLocal8Bit(argv[1])),nullptr,{args});
 check(bool(result),"decoration plugin loads"); std::unique_ptr<Decoration> d(result.plugin);
 d->setSettings(settings); d->create();
 QObject::connect(d.get(),&Decoration::nextStateChanged,d.get(),[&](const auto &state){d->apply(state);});
 check(d->init(),"decoration initializes");
 auto checkSection=[&](QPointF point,Qt::WindowFrameSection section,const char *message) {
  QHoverEvent event(QEvent::HoverMove,point,point,QPointF(-1,-1));
  QCoreApplication::sendEvent(d.get(),&event);
  check(d->sectionUnderMouse()==section,message);
 };
 checkSection(QPointF(d->size().width()/2,2),Qt::TopSection,"top edge allows vertical resize");
 checkSection(QPointF(2,2),Qt::TopLeftSection,"top left corner allows diagonal resize");
 checkSection(QPointF(d->size().width()-2,2),Qt::TopRightSection,"top right corner allows diagonal resize");
 checkSection(QPointF(d->size().width()/2,20),Qt::TitleBarArea,"titlebar interior still moves the window");
 bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=true;
 d->window()->maximizedChanged(true);
 checkSection(QPointF(d->size().width()/2,2),Qt::TitleBarArea,"maximized titlebar reaches the screen edge");
 bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=false;
 d->window()->maximizedChanged(false);
 checkSection(QPointF(d->size().width()/2,2),Qt::TopSection,"restoring the window restores top resize");
 QHoverEvent leave(QEvent::HoverLeave,QPointF(-1,-1),QPointF(-1,-1),QPointF(-1,-1));
 QCoreApplication::sendEvent(d.get(),&leave);
 check(!d->property("egPreview").toBool(),"live bridge does not enable local preview rendering");
 check(!EmeraldGlass::loadMaterialConfig().desktopBackdropForMaximized,"desktop backdrop defaults off");
 for(bool enabled : {true,false}) {
  cfg.desktopBackdropForMaximized=enabled; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
  check(EmeraldGlass::loadMaterialConfig().desktopBackdropForMaximized==enabled,"desktop backdrop config round trip");
  for(bool active : {true,false}) for(bool horizontal : {true,false}) for(bool vertical : {true,false}) {
   // Establish activation independently, then change only maximization.
   bridge.client->active=active; d->window()->activeChanged(active);
   bridge.client->maximizedHorizontally=horizontal; bridge.client->maximizedVertically=vertical;
   d->window()->maximizedChanged(horizontal && vertical);
   check(d->property("egDesktopBackdrop").toBool()==(enabled && active && horizontal && vertical),
         "maximization alone updates desktop source, including partial maximization");
   bridge.client->active=!active; d->window()->activeChanged(!active);
   check(d->property("egDesktopBackdrop").toBool()==(enabled && !active && horizontal && vertical),
         "activation alone updates desktop source without a geometry change");
  }
 }
 bridge.client->active=true; d->window()->activeChanged(true);
 const QRegion frame=d->property("egFrameRegion").value<QRegion>();
 check(!frame.isEmpty(),"material frame is published");
 const QRegion client(d->rect().adjusted(d->borderLeft(),d->borderTop(),-d->borderRight(),-d->borderBottom()).toAlignedRect());
 check((frame & client).isEmpty(),"material excludes client content");
 check(d->blurRegion()==frame,"standard blur handles fallback frame");
 d->setProperty("egShader",true);
 check(d->blurRegion().isEmpty(),"no standard blur over shader frame");
 check(d->property("egFrameRegion").value<QRegion>()==frame,"material region survives blur handoff");
 auto nativeImage=[&] {
  QImage image(d->size().toSize(),QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
  QPainter painter(&image); d->paint(&painter,d->rect()); painter.end(); return image;
 };
 // A transformation snapshot contains only the material. Native caption/halo
 // remain live, while standard Blur must not bring back the lower windows.
 const auto beforeSnapshot=cfg;
 cfg.desktopBackdropForMaximized=true;
 EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=true;
 d->window()->maximizedChanged(true);
 QImage snapshot(500,cfg.titleHeight,QImage::Format_ARGB32_Premultiplied);
 snapshot.fill(QColor("#2475df"));
 d->setProperty("egDesktopSnapshot",snapshot); d->setProperty("egTransformed",true);
 check(d->property("egDesktopBackdrop").toBool() && d->blurRegion().isEmpty(),
       "desktop snapshot suppresses standard blur during transformation");
 check(nativeImage().pixelColor(250,20)==QColor("#2475df"),
       "optical snapshot is painted without a second native material coat");
 check(nativeImage().pixelColor(250,100).alpha()==0,"desktop snapshot excludes client content");
 cfg.shaderEffectEnabled=false; cfg.frameOpacity=.2f;
 EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 const auto basicSnapshot=nativeImage();
 check(d->blurRegion().isEmpty() && basicSnapshot.pixelColor(250,20).alpha()==255,
       "non-optical snapshot supplies an opaque desktop beneath native material");
 snapshot.fill(QColor("#df7524")); d->setProperty("egDesktopSnapshot",snapshot);
 check(nativeImage().pixelColor(250,20)!=basicSnapshot.pixelColor(250,20),
       "non-optical native material transmits its desktop snapshot");
 bridge.client->active=false; d->window()->activeChanged(false);
 check(!d->blurRegion().isEmpty(),"inactive transformed window resumes normal blur despite an old snapshot");
 bridge.client->active=true; d->window()->activeChanged(true);
 check(d->blurRegion().isEmpty(),"active maximized snapshot resumes without standard blur");
 d->setProperty("egDesktopSnapshot",QVariant());
 check(!d->blurRegion().isEmpty(),"missing snapshot retains native fallback");
 d->setProperty("egDesktopSnapshot",snapshot); d->setProperty("egShader",false);
 check(!d->blurRegion().isEmpty(),"effect unload ignores retained snapshot");
 d->setProperty("egDesktopSnapshot",QVariant()); d->setProperty("egTransformed",false);
 bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=false;
 d->window()->maximizedChanged(false);
 cfg=beforeSnapshot; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 d->setProperty("egShader",true);
 auto hover=[&](QEvent::Type type,QPointF point,QPointF previous) {
  QHoverEvent event(type,point,point,previous); QCoreApplication::sendEvent(d.get(),&event);
 };
 cfg.frameOpacity=.2f; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 hover(QEvent::HoverEnter,QPointF(250,20),QPointF(-1,-1));
 const QImage topGlow=nativeImage();
 check(qAlpha(topGlow.pixel(250,20))>=60,"signature glow is visible over GPU glass without grabbing");
 check(qGreen(topGlow.pixel(250,20))>qRed(topGlow.pixel(250,20)),"hover glow uses the border accent");
 hover(QEvent::HoverMove,QPointF(3,150),QPointF(250,20));
 const QImage sideGlow=nativeImage();
 check(qAlpha(sideGlow.pixel(3,150))>=60,"cursor glow follows side border");
 check(qAlpha(sideGlow.pixel(250,20))==0,"old glow position clears");
 check(qAlpha(sideGlow.pixel(10,150))==0,"cursor glow does not enter client content");
 hover(QEvent::HoverLeave,QPointF(-1,-1),QPointF(3,150));
 check(qAlpha(nativeImage().pixel(3,150))==0,"cursor glow clears on leave");
 hover(QEvent::HoverEnter,QPointF(250,20),QPointF(-1,-1));
 cfg.hoverGlow=0; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 check(qAlpha(nativeImage().pixel(250,20))==0,"zero hover strength disables signature glow");
 hover(QEvent::HoverLeave,QPointF(-1,-1),QPointF(250,20));
 cfg.hoverGlow=1; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 const QImage shaderGlyphs=nativeImage();
 d->setProperty("egTransformed",true);
 check(d->blurRegion()==frame,"transformed window restores native material and standard blur region");
 const QImage transformedMaterial=nativeImage();
 const QRect button=d->property("egButtons").toList().first().toRectF().toAlignedRect();
 auto buttonAlpha=[&](const QImage &image) {
  int alpha=0; for(int y=button.top();y<=button.bottom();++y) for(int x=button.left();x<=button.right();++x) alpha+=qAlpha(image.pixel(x,y)); return alpha;
 };
 check(buttonAlpha(transformedMaterial)>buttonAlpha(shaderGlyphs)*2,"transformed snapshot contains button lens, not only glyph");
 d->setProperty("egPointer",QPointF(40,15));
 check(d->blurRegion()==frame,"pointer movement preserves transformed material mode");
 d->setProperty("egTransformed",false);
 check(d->blurRegion().isEmpty(),"settled window resumes shader without duplicate blur");
 d->setProperty("egShader",false);
 check(d->blurRegion()==frame,"effect unload restores fallback region");
 d->setProperty("egShader",true); cfg.shaderEffectEnabled=false; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 check(d->blurRegion().isEmpty(),"disabled optics keeps custom border blur without duplicate standard blur");
 check(d->property("egEnabled").toBool() && !d->property("egOptics").toBool(),"border blur and Liquid Glass optics have independent state");
 check(buttonAlpha(nativeImage())>buttonAlpha(shaderGlyphs)*2,"disabled optics keeps native button material above blur");
 d->setProperty("egShader",false);
 check(d->blurRegion()==frame,"effect unload with optics disabled restores standard blur");
 // Active tint must not leak into inactive CPU buttons.
 bridge.client->active=false; d->window()->activeChanged(false);
 cfg.tint=0; cfg.inactiveTint=0; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 const auto inactiveBefore=nativeImage();
 cfg.tint=2; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 check(nativeImage()==inactiveBefore,"inactive CPU material ignores active tint");
 cfg.inactiveTint=2; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 check(nativeImage()!=inactiveBefore,"inactive CPU material responds to inactive tint");
 bridge.client->active=true; d->window()->activeChanged(true);
 bridge.client->active=true; d->window()->activeChanged(true);
 const auto idleButtons=nativeImage();
 const auto closeCenter=d->property("egButtons").toList().last().toRectF().center();
 hover(QEvent::HoverEnter,closeCenter,QPointF(-1,-1));
 check(nativeImage()!=idleButtons,"CPU button cache responds to hover and halo");
 hover(QEvent::HoverLeave,QPointF(-1,-1),closeCenter);
 check(nativeImage()==idleButtons,"CPU button cache clears hover state");
 // Compare the reusable edge cache with the full SDF reference, including
 // fractional scales and client corners, away from caption/button glyphs.
 const auto savedGeometry=cfg;
 bridge.client->active=true; d->window()->activeChanged(true);
 for(const auto geometry : {QPoint(26,0),QPoint(39,8),QPoint(56,20)}) {
  cfg.titleHeight=geometry.x(); cfg.borderWidth=geometry.y(); cfg.cornerRadius=geometry.y()==20 ? 18 : 5;
  EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
  for(qreal scale : {1.,1.25,1.5,2.}) {
   const QSize pixels=(d->size()*scale).toSize();
   QImage actual(pixels,QImage::Format_ARGB32_Premultiplied); actual.setDevicePixelRatio(scale); actual.fill(Qt::transparent);
   QPainter painter(&actual); d->paint(&painter,d->rect()); painter.end();
   const QRectF content=d->rect().adjusted(d->borderLeft(),d->borderTop(),-d->borderRight(),-d->borderBottom());
   const auto reference=EmeraldGlass::renderFrameGlassImage(pixels,scale,d->rect(),content,cfg.colorForWindow(true),cfg.tint,cfg.cornerRadius,cfg);
   int maximum=0; QPoint worst;
   for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x) {
    const qreal lx=(x+.5)/scale,ly=(y+.5)/scale;
    if(ly<cfg.titleHeight && lx>14 && lx<250) continue;
    bool buttonPixel=false;
    for(const auto &button:d->property("egButtons").toList()) buttonPixel |= button.toRectF().adjusted(-1,-1,1,1).contains(QPointF(lx,ly));
    if(buttonPixel) continue;
    if(ly>6 && ly<d->size().height()-d->borderBottom()-2 && lx>12 && lx<d->size().width()-12) continue;
    auto a=actual.pixel(x,y),b=reference.pixel(x,y);
    for(int shift:{0,8,16,24}) if(int delta=std::abs(int((a>>shift)&255)-int((b>>shift)&255)); delta>maximum) {maximum=delta;worst=QPoint(x,y);}
   }
   if(maximum>2) std::cerr<<"SDF cache difference: scale="<<scale<<" title="<<cfg.titleHeight<<" border="<<cfg.borderWidth<<" worst="<<worst.x()<<","<<worst.y()<<" maximum="<<maximum<<'\n';
   check(maximum<=2,"cached frame preserves full SDF contours and material");
  }
 }
 cfg=savedGeometry; EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 auto moduleResult=KPluginFactory::instantiatePlugin<KCModule>(KPluginMetaData(QString::fromLocal8Bit(argv[2])));
 check(bool(moduleResult),"KCM plugin loads"); std::unique_ptr<KCModule> module(moduleResult.plugin); module->load();
 auto *tabs=module->widget()->findChild<QTabWidget *>();
 check(tabs && tabs->currentIndex()==0,"KCM opens on first page");
 auto *blur=tabs->widget(0)->findChild<QSlider *>(QStringLiteral("frameBlurStrength"));
 check(blur && blur->minimum()==0 && blur->maximum()==15,"explicit blur control is on first page");
 check(blur->value()==15,"blur slider loads saved value");
 auto *desktopBackdrop=tabs->widget(0)->findChild<QCheckBox *>(QStringLiteral("desktopBackdropForMaximized"));
 check(desktopBackdrop && !desktopBackdrop->isChecked(),"desktop backdrop checkbox is on first page and defaults off");
 check(!desktopBackdrop->isEnabled(),"desktop backdrop requires the companion effect");
 for(bool enabled : {true,false}) {
  desktopBackdrop->setChecked(enabled); module->save(); module->load();
  check(EmeraldGlass::loadMaterialConfig().desktopBackdropForMaximized==enabled && desktopBackdrop->isChecked()==enabled,
        "desktop backdrop checkbox saves and reloads");
 }
 desktopBackdrop->setChecked(true); module->defaults();
 check(!desktopBackdrop->isChecked(),"Restore Defaults disables desktop backdrop");
 module->load();
 auto *optics=module->widget()->findChild<QGroupBox *>(QStringLiteral("liquidGlassControls"));
 check(optics && !optics->isEnabled(),"Liquid Glass controls stay disabled without a running effect");
 check(!blur->isEnabled(),"shader blur control is disabled without a running effect");
 auto *halo=tabs->widget(0)->findChild<QSlider *>(QStringLiteral("cursorGlowStrength"));
 check(halo && halo->isEnabled(),"signature cursor halo stays available without shader");
 auto *refraction=optics->findChild<QSlider *>(QStringLiteral("refractionStrength"));
 auto *dispersion=optics->findChild<QSlider *>(QStringLiteral("chromaticDispersion"));
 check(refraction && dispersion,"both optics sliders are identifiable");
 for (const auto values : {QPoint(0,0), QPoint(180,0), QPoint(180,100), QPoint(67,38)}) {
  refraction->setValue(values.x()); dispersion->setValue(values.y()); module->save(); settings->reconfigured();
  const auto saved=EmeraldGlass::loadMaterialConfig();
  check(qAbs(saved.refraction-values.x()/100.0f)<0.0001f && qAbs(saved.dispersion-values.y()/100.0f)<0.0001f,
        "optics sliders persist independent normalized values");
  check(qAbs(d->property("egRefraction").toFloat()-saved.refraction)<0.0001f &&
        qAbs(d->property("egDispersion").toFloat()-saved.dispersion)<0.0001f,
        "saved optics reach the decoration properties consumed by the effect");
 }
 auto *adaptive=optics->findChild<QSlider *>(QStringLiteral("adaptiveBackdropContrast"));
 check(adaptive,"adaptive backdrop contrast slider exists");
 for (int value : {0,100,37}) {
  adaptive->setValue(value); module->save(); settings->reconfigured();
  check(qAbs(EmeraldGlass::loadMaterialConfig().adaptiveContrast-value/100.0f)<0.0001f &&
        qAbs(d->property("egAdaptiveContrast").toFloat()-value/100.0f)<0.0001f,
        "adaptive contrast slider persists and reaches decoration shader property");
 }
 auto *activeTint=tabs->widget(0)->findChild<QSlider *>(QStringLiteral("activeTintStrength"));
 auto *inactiveTint=tabs->widget(0)->findChild<QSlider *>(QStringLiteral("inactiveTintStrength"));
 check(activeTint && inactiveTint && activeTint->maximum()==200 && inactiveTint->maximum()==200,
       "both tint sliders expose 200 percent headroom");
 for (const auto values : {QPoint(0,200), QPoint(200,0), QPoint(150,175), QPoint(100,100)}) {
  activeTint->setValue(values.x()); inactiveTint->setValue(values.y()); module->save(); settings->reconfigured();
  const auto saved=EmeraldGlass::loadMaterialConfig();
  check(qAbs(saved.tint-values.x()/100.f)<.0001f && qAbs(saved.inactiveTint-values.y()/100.f)<.0001f,
        "active and inactive tint independently persist the expanded range");
  check(qAbs(d->property("egTint").toFloat()-saved.tint)<.0001f,"expanded active tint reaches the effect property");
 }
 const EmeraldGlass::Math::Vec3 base{.18f,.24f,.20f}, accent{.08f,.66f,.27f};
 auto luma=[](auto c){return c.x*.299f+c.y*.587f+c.z*.114f;};
 const auto boosted=EmeraldGlass::Math::boostTint(base,accent,2);
 check(qAbs(luma(base)-luma(boosted))<.00001f && boosted.y-boosted.x>base.y-base.x,
       "native extra tint increases preset color at constant luminance");
 const auto unchanged=EmeraldGlass::Math::boostTint(base,accent,1);
 check(unchanged.x==base.x && unchanged.y==base.y && unchanged.z==base.z,"native tint below boost threshold is unchanged");
 // Red close feedback must not depend on the decorative cursor glow or tint.
 EmeraldGlass::MaterialConfig buttonConfig; buttonConfig.tint=0; buttonConfig.hoverGlow=0;
 for (bool pressed : {false,true}) {
  auto buttonImage=[&](bool close,bool hovered) {
   QImage image(40,36,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
   QPainter painter(&image);
   EmeraldGlass::paintButtonGlass(&painter,QRectF(5,5,27,23),1,QColor(20,240,60),buttonConfig.tint,close,hovered,pressed,QPointF(18,17),buttonConfig);
   painter.end(); return image;
  };
  const auto close=buttonImage(true,!pressed).pixelColor(18,18);
  check(close.red()>close.green()*2 && close.red()>close.blue()*2,"native close hover and press retain a clearly red glass body at zero tint and glow");
  check(buttonImage(false,false)==buttonImage(true,false) || pressed,"idle close preserves the common button material");
 }
 for (const QColor baseColor : {QColor("#f7140e"),QColor("#ff8c9d"),QColor("#78222d"),QColor("#ff9e14"),QColor("#17f559")}) {
  const bool warmClose=baseColor!=QColor("#ff9e14") && baseColor!=QColor("#17f559");
  const bool blueClose=baseColor==QColor("#ff9e14");
  for(bool pressed : {false,true}) for(float glow : {0.f,1.f}) {
   buttonConfig.hoverGlow=glow;
   QImage image(40,36,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
   QPainter painter(&image);
   EmeraldGlass::paintButtonGlass(&painter,QRectF(5,5,27,23),1,baseColor,buttonConfig.tint,true,!pressed,pressed,QPointF(18,17),buttonConfig);
   painter.end(); const auto pixel=image.pixelColor(18,18);
   check(blueClose ? pixel.blue()>pixel.red()*2 && pixel.blue()>pixel.green()*1.4
                  : pixel.red()>pixel.blue()*2 && (warmClose ? pixel.green()>pixel.blue()*2 && pixel.green()>pixel.red()*.4 : pixel.red()>pixel.green()*2),
         "native close uses blue on amber, gold on red/pink and red on emerald, in hover and press with any glow");
  }
 }
 auto *activeTitle=tabs->widget(0)->findChild<QComboBox *>(QStringLiteral("activeTitleText"));
 auto *inactiveTitle=tabs->widget(0)->findChild<QComboBox *>(QStringLiteral("inactiveTitleText"));
 check(activeTitle && inactiveTitle && activeTitle->count()==3 && inactiveTitle->count()==3,"independent title text modes are on the first page");
 for(const auto modes : {QPoint(0,2),QPoint(1,2),QPoint(2,1),QPoint(0,0)}) {
  activeTitle->setCurrentIndex(activeTitle->findData(modes.x())); inactiveTitle->setCurrentIndex(inactiveTitle->findData(modes.y())); module->save();
  const auto saved=EmeraldGlass::loadMaterialConfig();
  check(saved.activeTitleText==modes.x() && saved.inactiveTitleText==modes.y(),"active/inactive title modes independently round trip through KCM");
 }
 EmeraldGlass::MaterialConfig textConfig;
 textConfig.activePreset=textConfig.inactivePreset=QStringLiteral("custom");
 textConfig.activeCustomColor=textConfig.inactiveCustomColor=Qt::white;
 textConfig.tint=textConfig.inactiveTint=0; textConfig.frameOpacity=.2f; textConfig.adaptiveContrast=0;
 for(bool optical : {false,true}) {
  check(EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(true,optical))<.1f,"auto uses dark text on a light translucent material");
  textConfig.frameOpacity=1;
  check(EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(true,optical))>.5f,"opaque light-preset material uses light text because its actual coat is dark");
  textConfig.frameOpacity=.2f;
 }
 textConfig.activeCustomColor=Qt::black;
 check(EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(true,true))>.5f,"auto uses light text on a dark material");
 check(EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(false,true))<.1f,"inactive auto uses its own preset independently");
 textConfig.activeTitleText=2; textConfig.inactiveTitleText=1;
 check(EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(true,true))<.1f &&
       EmeraldGlass::relativeLuminance(textConfig.titleColorForWindow(false,true))>.5f,"manual text modes override automatic contrast independently");
 const auto darkTitle=textConfig.titleColorForWindow(true,true);
 check(EmeraldGlass::relativeLuminance(EmeraldGlass::titleShadowColor(darkTitle))>.5f,"dark text receives a light shadow");
 cfg=EmeraldGlass::loadMaterialConfig(); cfg.activeTitleText=1; cfg.inactiveTitleText=2; cfg.shaderEffectEnabled=true;
 EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured(); d->setProperty("egShader",true);
 const auto lightCaption=nativeImage();
 bridge.client->active=false; d->window()->activeChanged(false);
 const auto darkCaption=nativeImage();
 auto captionBrightness=[](const QImage &image) {
  double brightness=0; int samples=0;
  for(int y=7;y<31;++y) for(int x=10;x<250;++x) {
   const auto pixel=image.pixelColor(x,y);
   if(pixel.alpha()>128) {brightness+=pixel.red()+pixel.green()+pixel.blue(); ++samples;}
  }
  check(samples>10,"native caption has measurable ink"); return brightness/(samples*3);
 };
 check(captionBrightness(lightCaption)>captionBrightness(darkCaption)+30,"native painting switches caption ink with active/inactive text settings");
 bridge.client->active=true; d->window()->activeChanged(true);
 cfg.activeTitleText=0; cfg.activePreset=QStringLiteral("custom"); cfg.activeCustomColor=Qt::white;
 cfg.frameOpacity=.6f; cfg.tint=0; cfg.adaptiveContrast=1;
 EmeraldGlass::saveMaterialConfig(cfg); settings->reconfigured();
 const auto steadyInk=nativeImage(); d->setProperty("egTransformed",true); const auto movingInk=nativeImage();
 int opaqueInk=0;
 for(int y=7;y<31;++y) for(int x=10;x<250;++x) {
  const auto before=steadyInk.pixelColor(x,y);
  if(before.alpha()==255 && before==QColor("#effff6")) {
   ++opaqueInk; check(movingInk.pixelColor(x,y)==before,"auto ink stays stable during native deformation fallback");
  }
 }
 check(opaqueInk>0,"deformation comparison exercises opaque caption glyphs");
 d->setProperty("egTransformed",false);
 blur->setValue(0); module->save();
 check(EmeraldGlass::loadMaterialConfig().blurStrength==0,"blur slider saves zero");
 module->load();
 bool zeroLabel=false;
 for(auto *label:blur->parentWidget()->findChildren<QLabel *>()) zeroLabel |= label->text()==QStringLiteral("0 / 15");
 check(zeroLabel,"saved zero blur has a numeric label immediately after loading");
 // The native fallback must retain Crystal's blue material under the same
 // highlight/hover settings as the production-shader regression.
 EmeraldGlass::MaterialConfig crystal; crystal.specular=.58f; crystal.hoverGlow=.88f;
 for(qreal dpr : {1.,1.25,1.5,2.}) {
  QImage image(QSize(qRound(40*dpr),qRound(36*dpr)),QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(dpr); image.fill(QColor(26,82,97));
  QPainter painter(&image);
  EmeraldGlass::paintButtonGlass(&painter,QRectF(5,5,27,23),dpr,QColor("#a8e4f5"),1.02f,false,true,false,QPointF(18.5,16.5),crystal);
  painter.end(); int white=0,samples=0;
  for(int y=qRound(5*dpr);y<qRound(28*dpr);++y) for(int x=qRound(5*dpr);x<qRound(32*dpr);++x) {
   const auto color=image.pixelColor(x,y); ++samples; white+=std::min({color.red(),color.green(),color.blue()})>205;
  }
  check(white<samples/50,"native Crystal hover keeps its blue tint without white washout");
 }
 {
  // Button shape configuration, KCM selector, geometry invariants, and StealthLens CPU contrast.
  EmeraldGlass::MaterialConfig shapeCfg;
  check(shapeCfg.buttonShape==0,"button shape defaults to Classic Emerald");
  for(int shape=0; shape<=4; ++shape) {
   shapeCfg.buttonShape=shape; EmeraldGlass::saveMaterialConfig(shapeCfg);
   check(EmeraldGlass::loadMaterialConfig().buttonShape==shape,"button shape round-trips");
  }
  {
   KConfigGroup geom(KSharedConfig::openConfig(QStringLiteral("emeraldglassrc")),QStringLiteral("Geometry"));
   geom.writeEntry("ButtonShape",99); geom.sync();
   check(EmeraldGlass::loadMaterialConfig().buttonShape==4,"button shape clamps out-of-range high values");
   geom.writeEntry("ButtonShape",-3); geom.sync();
   check(EmeraldGlass::loadMaterialConfig().buttonShape==0,"button shape clamps negative values");
  }
  auto *buttonShapeCombo=tabs->widget(2)->findChild<QComboBox *>(QStringLiteral("buttonShape"));
  check(buttonShapeCombo && buttonShapeCombo->count()==5,"button shape selector is on Geometry & Buttons tab with 5 shapes");
  buttonShapeCombo->setCurrentIndex(buttonShapeCombo->findData(int(EmeraldGlass::ButtonShape::JoinedStrip)));
  module->save(); settings->reconfigured();
  check(EmeraldGlass::loadMaterialConfig().buttonShape==int(EmeraldGlass::ButtonShape::JoinedStrip),"KCM saves JoinedStrip button shape");
  check(d->property("egButtonShape").toInt()==int(EmeraldGlass::ButtonShape::JoinedStrip),"decoration publishes egButtonShape");
  {
   const auto btns=d->property("egButtons").toList();
   const auto radii=d->property("egButtonRadii").toList();
   check(btns.size()==3 && radii.size()==3,"default JoinedStrip publishes 3 buttons and radii");
   const QRectF b0=btns[0].toRectF(), b1=btns[1].toRectF(), b2=btns[2].toRectF();
   check(qFuzzyCompare(b0.width(),26.0) && qFuzzyCompare(b1.width(),26.0) && qFuzzyCompare(b2.width(),35.0),
         "JoinedStrip widens Close to 35px and keeps others 26px");
   check(qFuzzyCompare(b0.right(),b1.left()) && qFuzzyCompare(b1.right(),b2.left()),
         "JoinedStrip has zero gap between adjacent buttons");
   const QVector4D r0=radii[0].value<QVector4D>(), r1=radii[1].value<QVector4D>(), r2=radii[2].value<QVector4D>();
   check(qFuzzyCompare(r0.x(),5.0f) && qFuzzyCompare(r0.y(),0.5f) && qFuzzyCompare(r1.x(),0.5f) &&
         qFuzzyCompare(r2.x(),0.5f) && qFuzzyCompare(r2.y(),5.0f),
         "JoinedStrip rounds only outer corners and joins inner corners");
  }
  shapeCfg=EmeraldGlass::loadMaterialConfig();
  shapeCfg.cornerRadius=18; shapeCfg.buttonShape=int(EmeraldGlass::ButtonShape::TopDockedTabs);
  EmeraldGlass::saveMaterialConfig(shapeCfg); settings->reconfigured();
  {
   const auto btns=d->property("egButtons").toList();
   const auto radii=d->property("egButtonRadii").toList();
   check(btns.size()==3 && radii.size()==3,"TopDockedTabs publishes 3 buttons and radii");
   const QRectF b0=btns[0].toRectF(), b2=btns[2].toRectF();
   check(qFuzzyCompare(b0.top(),1.0) && d->rect().right()-b2.right()>=20.0-0.01,
         "TopDockedTabs docks at y=1 when restored and respects large corner radius inset");
   const QVector4D r0=radii[0].value<QVector4D>(), r2=radii[2].value<QVector4D>();
   check(qFuzzyCompare(r0.x(),0.5f) && qFuzzyCompare(r0.w(),5.5f) && qFuzzyCompare(r2.y(),0.5f) && qFuzzyCompare(r2.z(),5.5f),
         "TopDockedTabs keeps top corners square and rounds bottom outer corners");
   check(qFuzzyCompare(d->resizeOnlyBorderTop(),4.0),"TopDockedTabs sets 4px outer top resize border");
   checkSection(QPointF(b2.center().x(),2.0),Qt::TitleBarArea,"top of TopDockedTabs button is inside titleBar, not resize");
   checkSection(QPointF(d->size().width()/2,0.5),Qt::TopSection,"1px top rim still resizes when TopDockedTabs is active");
   bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=true;
   d->window()->maximizedChanged(true);
   check(qFuzzyCompare(d->property("egButtons").toList()[0].toRectF().top(),0.0),"TopDockedTabs reaches y=0 when maximized");
   check(qFuzzyCompare(d->resizeOnlyBorderTop(),0.0),"maximized TopDockedTabs clears resizeOnlyBorders");
   bridge.client->maximizedHorizontally=bridge.client->maximizedVertically=false;
   d->window()->maximizedChanged(false);
  }
  buttonShapeCombo->setCurrentIndex(buttonShapeCombo->findData(int(EmeraldGlass::ButtonShape::LiquidCapsules)));
  module->save(); settings->reconfigured();
  {
   const auto btns=d->property("egButtons").toList();
   const auto radii=d->property("egButtonRadii").toList();
   check(btns.size()==3 && qFuzzyCompare(btns[0].toRectF().width(),29.0) && qFuzzyCompare(btns[0].toRectF().height(),22.0),
         "LiquidCapsules uses 29x22 pill geometry");
   check(qFuzzyCompare(btns[1].toRectF().left()-btns[0].toRectF().right(),4.0),"LiquidCapsules uses 4px spacing");
   check(qFuzzyCompare(radii[0].value<QVector4D>().x(),11.0f),"LiquidCapsules uses 11px pill corner radius");
  }
  {
   EmeraldGlass::MaterialConfig stealthCfg;
   stealthCfg.buttonShape=int(EmeraldGlass::ButtonShape::StealthLens);
   auto renderCpuBtn=[&](const EmeraldGlass::MaterialConfig &c,bool hov) {
    QImage img(40,36,QImage::Format_ARGB32_Premultiplied); img.fill(Qt::transparent);
    QPainter painter(&img);
    EmeraldGlass::paintButtonGlass(&painter,QRectF(5,5,27,23),1,QColor("#17f559"),0.7f,false,hov,false,QPointF(18,16),c,EmeraldGlass::ButtonRadii{5,5,5,5});
    painter.end();
    return img;
   };
   const QImage stealthIdle=renderCpuBtn(stealthCfg,false);
   const QImage stealthHover=renderCpuBtn(stealthCfg,true);
   EmeraldGlass::MaterialConfig classicCfg; classicCfg.buttonShape=int(EmeraldGlass::ButtonShape::Classic);
   const QImage classicIdle=renderCpuBtn(classicCfg,false);
   check(stealthIdle!=classicIdle,"StealthLens idle differs from Classic");
   check(stealthIdle.pixelColor(18,16).alpha()*2 < stealthHover.pixelColor(18,16).alpha(),
         "StealthLens idle is visibly more translucent than hover lens");
  }
  module->defaults();
  check(buttonShapeCombo->currentData().toInt()==0,"Restore Defaults resets button shape to Classic Emerald");
 }
 {
  // Button color modes, KCM selector, improved HSV Close accent, and hue avoidance.
  EmeraldGlass::MaterialConfig colorCfg;
  check(colorCfg.buttonColorMode==0,"button color mode defaults to Monochrome");
  for(int mode=0; mode<=4; ++mode) {
   colorCfg.buttonColorMode=mode; EmeraldGlass::saveMaterialConfig(colorCfg);
   check(EmeraldGlass::loadMaterialConfig().buttonColorMode==mode,"button color mode round-trips");
  }
  {
   KConfigGroup geom(KSharedConfig::openConfig(QStringLiteral("emeraldglassrc")),QStringLiteral("Geometry"));
   geom.writeEntry("ButtonColorMode",77); geom.sync();
   check(EmeraldGlass::loadMaterialConfig().buttonColorMode==4,"button color mode clamps out-of-range high values");
   geom.writeEntry("ButtonColorMode",-5); geom.sync();
   check(EmeraldGlass::loadMaterialConfig().buttonColorMode==0,"button color mode clamps negative values");
  }
  auto *colorModeCombo=tabs->widget(2)->findChild<QComboBox *>(QStringLiteral("buttonColorMode"));
  check(colorModeCombo && colorModeCombo->count()==5,"button color selector is on Geometry & Buttons tab with 5 modes");
  colorModeCombo->setCurrentIndex(colorModeCombo->findData(int(EmeraldGlass::ButtonColorMode::AdaptiveJewelTrio)));
  module->save(); settings->reconfigured();
  check(EmeraldGlass::loadMaterialConfig().buttonColorMode==int(EmeraldGlass::ButtonColorMode::AdaptiveJewelTrio),
        "KCM saves AdaptiveJewelTrio button color mode");
  const auto colors=d->property("egButtonColors").toList();
  const auto meta=d->property("egButtonColorMeta").toList();
  check(colors.size()==3 && meta.size()==3,"decoration publishes egButtonColors and egButtonColorMeta");
  const QVector4D minCol=colors[0].value<QVector4D>(), maxCol=colors[1].value<QVector4D>(), clsCol=colors[2].value<QVector4D>();
  check(minCol.w()>1.0f && maxCol.w()>1.0f && clsCol.w()>1.0f,"AdaptiveJewelTrio publishes active fillBoost");
  check(clsCol.x()>clsCol.y()*2.0f && maxCol.z()>maxCol.x()*1.5f,"AdaptiveJewelTrio keeps Close red and shifts Max away from green");

  auto closeRgb=[](const QColor &c) {
   return EmeraldGlass::Math::closeButtonAccent({float(c.redF()),float(c.greenF()),float(c.blueF())});
  };
  const auto pinkClose=closeRgb(QColor("#ff1493"));
  check(pinkClose.x>0.8f && pinkClose.y>0.5f && pinkClose.z<0.3f,"Hot Pink close uses warm gold instead of red");
  const auto orangeClose=closeRgb(QColor("#ff5500"));
  check(orangeClose.z>0.8f && orangeClose.x<0.3f,"Bright Orange close uses cobalt blue");
  const auto amethystClose=closeRgb(QColor("#b833ff"));
  check(amethystClose.x>0.8f && amethystClose.y<0.2f && amethystClose.z<0.2f,"Amethyst close uses ruby red");
  const auto crystalClose=closeRgb(QColor("#a8e4f5"));
  check(crystalClose.x>0.8f && crystalClose.y<0.2f && crystalClose.z<0.2f,"Crystal close uses ruby red");

  // AdaptiveJewelTrio hue-avoidance across Emerald, Ruby, Amber, Sapphire, Crystal
  for(const QColor themeCol : {QColor("#17f559"),QColor("#f7140e"),QColor("#ff9e14"),QColor("#1e8fff"),QColor("#a8e4f5")}) {
   const auto themeHsv=EmeraldGlass::Math::rgbToHsv({float(themeCol.redF()),float(themeCol.greenF()),float(themeCol.blueF())});
   for(auto role : {EmeraldGlass::ButtonColorRole::Minimize,EmeraldGlass::ButtonColorRole::Maximize,EmeraldGlass::ButtonColorRole::Close}) {
    const auto spec=EmeraldGlass::resolveButtonColor(themeCol,0.7f,int(EmeraldGlass::ButtonColorMode::AdaptiveJewelTrio),role,false,false,true);
    const auto btnHsv=EmeraldGlass::Math::rgbToHsv({float(spec.color.redF()),float(spec.color.greenF()),float(spec.color.blueF())});
    if(themeHsv.s>0.38f) {
     const float dh=std::min(std::abs(themeHsv.h-btnHsv.h),360.0f-std::abs(themeHsv.h-btnHsv.h));
     check(dh>=28.0f,"AdaptiveJewelTrio keeps each button hue distinct from saturated titlebar");
    }
   }
  }
  // HarmonicHsl complementary Close contrast on arbitrary custom color
  const QColor tealCustom("#18b8a8");
  const auto harmClose=EmeraldGlass::resolveButtonColor(tealCustom,0.7f,int(EmeraldGlass::ButtonColorMode::HarmonicHsl),EmeraldGlass::ButtonColorRole::Close,false,false,true);
  const auto tealHsv=EmeraldGlass::Math::rgbToHsv({float(tealCustom.redF()),float(tealCustom.greenF()),float(tealCustom.blueF())});
  const auto harmHsv=EmeraldGlass::Math::rgbToHsv({float(harmClose.color.redF()),float(harmClose.color.greenF()),float(harmClose.color.blueF())});
  const float harmDh=std::min(std::abs(tealHsv.h-harmHsv.h),360.0f-std::abs(tealHsv.h-harmHsv.h));
  check(harmDh>140.0f,"HarmonicHsl gives Close a strong complementary hue contrast");

  // SmokedChrome neutral idle vs colorful hover
  const auto smokedIdle=EmeraldGlass::resolveButtonColor(QColor("#17f559"),0.7f,int(EmeraldGlass::ButtonColorMode::SmokedChrome),EmeraldGlass::ButtonColorRole::Minimize,false,false,true);
  const auto smokedHover=EmeraldGlass::resolveButtonColor(QColor("#17f559"),0.7f,int(EmeraldGlass::ButtonColorMode::SmokedChrome),EmeraldGlass::ButtonColorRole::Minimize,true,false,true);
  const float idleChroma=std::max({smokedIdle.color.redF(),smokedIdle.color.greenF(),smokedIdle.color.blueF()})-std::min({smokedIdle.color.redF(),smokedIdle.color.greenF(),smokedIdle.color.blueF()});
  const float hoverChroma=std::max({smokedHover.color.redF(),smokedHover.color.greenF(),smokedHover.color.blueF()})-std::min({smokedHover.color.redF(),smokedHover.color.greenF(),smokedHover.color.blueF()});
  check(idleChroma<0.25f && hoverChroma>idleChroma*2.5f && smokedIdle.effectiveTint<smokedHover.effectiveTint,
        "SmokedChrome stays neutral when idle and pops color on hover");

  // Yellow #ffee00 in AdaptiveJewelTrio must produce red Close consistent with closeButtonAccent.
  const auto yellowClose=EmeraldGlass::resolveButtonColor(QColor("#ffee00"),0.7f,int(EmeraldGlass::ButtonColorMode::AdaptiveJewelTrio),EmeraldGlass::ButtonColorRole::Close,false,false,true);
  check(yellowClose.color.red()>200 && yellowClose.color.green()<60 && yellowClose.color.blue()<60,
        "AdaptiveJewelTrio yellow uses red Close matching closeButtonAccent");

  // Button glyph contrast: dark forced title on opaque Emerald keeps light button glyphs,
  // while low-opacity pale glass uses dark button glyphs.
  EmeraldGlass::MaterialConfig glyphCfg;
  glyphCfg.activeTitleText=2; // Dark forced
  glyphCfg.frameOpacity=0.84f;
  const auto emeraldSpec=EmeraldGlass::resolveButtonColor(QColor("#17f559"),0.7f,0,EmeraldGlass::ButtonColorRole::Minimize,false,false,true);
  check(EmeraldGlass::relativeLuminance(EmeraldGlass::buttonGlyphColor(emeraldSpec,glyphCfg,true,true,true,false))>0.5f,
        "opaque Emerald keeps light button glyphs even with forced dark title");
  glyphCfg.activePreset=QStringLiteral("custom"); glyphCfg.activeCustomColor=Qt::white;
  glyphCfg.frameOpacity=0.25f; glyphCfg.tint=0.0f; glyphCfg.activeTitleText=0;
  const auto whiteSpec=EmeraldGlass::resolveButtonColor(Qt::white,0.0f,0,EmeraldGlass::ButtonColorRole::Minimize,false,false,true);
  check(EmeraldGlass::relativeLuminance(EmeraldGlass::buttonGlyphColor(whiteSpec,glyphCfg,true,true,true,false))<0.1f,
        "low-opacity pale glass switches button glyphs to dark ink");

  module->defaults();
  check(colorModeCombo->currentData().toInt()==0,"Restore Defaults resets button color mode to Monochrome");
 }
 std::cout<<"PASS: blur config, first-page blur slider, frame-only mask, shader/standard blur handoff, unload, disable\n";
}
