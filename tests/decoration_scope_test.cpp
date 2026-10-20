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
#include <QSlider>
#include <QTabWidget>
#include <QTemporaryDir>
#include <iostream>
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
 check(!d->property("egPreview").toBool(),"live bridge does not enable local preview rendering");
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
  for(bool pressed : {false,true}) for(float glow : {0.f,1.f}) {
   buttonConfig.hoverGlow=glow;
   QImage image(40,36,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
   QPainter painter(&image);
   EmeraldGlass::paintButtonGlass(&painter,QRectF(5,5,27,23),1,baseColor,buttonConfig.tint,true,!pressed,pressed,QPointF(18,17),buttonConfig);
   painter.end(); const auto pixel=image.pixelColor(18,18);
   check(pixel.red()>pixel.blue()*2,"native close warning keeps a warm visible color");
   check(warmClose ? pixel.green()>pixel.blue()*2 && pixel.green()>pixel.red()*.4 : pixel.red()>pixel.green()*2,
         "native close uses gold on red/pink presets and red on emerald/amber, in hover and press with any glow");
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
 std::cout<<"PASS: blur config, first-page blur slider, frame-only mask, shader/standard blur handoff, unload, disable\n";
}
