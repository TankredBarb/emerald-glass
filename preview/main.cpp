// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLPaintDevice>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QPainter>
#include <QSlider>
#include <QSurfaceFormat>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector3D>
#include <QVector4D>
#include <array>
#include <cstdio>
#include <memory>
#ifdef HAS_MATERIAL_PREVIEW
#include "../native/materialpreview.h"
int runSheetComparison(QApplication &app);
int runCloseComparison(QApplication &app);
#include <KCModule>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QTabWidget>
#include <QGroupBox>
#endif

struct GlassWindow {
    QRectF rect;
    QVector3D color;
    QString title;
    bool terminal;
};

class Preview : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
public:
    explicit Preview(QWidget *parent = nullptr) : QOpenGLWidget(parent)
    {
        setMinimumSize(980, 570);
        setMouseTracking(true);
        windows = {{{QRectF(460, 210, 480, 315), {0.97f, 0.08f, 0.055f}, QStringLiteral("RUBY — Red Glass"), true},
                    {QRectF(72, 65, 560, 350), {0.09f, 0.96f, 0.35f}, QStringLiteral("EMERALD — Green Glass"), false}}};
        connect(&timer, &QTimer::timeout, this, qOverload<>(&Preview::update));
        timer.start(33);
        elapsed.start();
    }

    ~Preview() override
    {
        makeCurrent();
        buffers = {};
        glass.reset();
        background.reset();
        glDeleteVertexArrays(1, &vao);
        doneCurrent();
    }

    void setRefraction(int value) { refraction = value / 100.0f; update(); }
    void setTint(int value) { tint = value / 100.0f; update(); }
    void setDispersion(int value) { dispersion = value / 100.0f; update(); }
    void setLight(bool value) { light = value; update(); }
    void setAnimation(bool enabled)
    {
        if (enabled) {
            elapsed.restart();
            timer.start(33);
        } else {
            frozenTime += elapsed.elapsed() / 1000.0f;
            timer.stop();
        }
        update();
    }

protected:
    void initializeGL() override
    {
        if (!initializeOpenGLFunctions()) qFatal("OpenGL 3.3 is required");
        std::fprintf(stderr, "GL renderer: %s\n", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        std::fprintf(stderr, "GL version: %s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));
        glGenVertexArrays(1, &vao);
        background = program("background.frag");
        glass = program("glass.frag");
    }

    void paintGL() override
    {
        const qreal dpr = devicePixelRatioF();
        const QSize pixels(qRound(width() * dpr), qRound(height() * dpr));
        if (!buffers[0] || buffers[0]->size() != pixels) {
            for (auto &buffer : buffers) {
                buffer = std::make_unique<QOpenGLFramebufferObject>(pixels);
                if (!buffer->isValid()) qFatal("Could not create preview framebuffer");
                glBindTexture(GL_TEXTURE_2D, buffer->texture());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
        }

        buffers[0]->bind();
        prepare(pixels);
        background->bind();
        background->setUniformValue("resolution", QVector2D(width(), height()));
        background->setUniformValue("time", frozenTime + (timer.isActive() ? elapsed.elapsed() / 1000.0f : 0.0f));
        background->setUniformValue("lightBackground", light ? 1 : 0);
        triangle();
        background->release();

        int current = 0;
        const auto [hitWindow, hitButton] = hit(pointer);
        for (int index = 0; index < int(windows.size()); ++index) {
            const GlassWindow &window = windows[index];
            const int next = 1 - current;
            buffers[next]->bind();
            prepare(pixels);
            glass->bind();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, buffers[current]->texture());
            glass->setUniformValue("kwinMode", 0);
            glass->setUniformValue("scene", 0);
            glass->setUniformValue("resolution", QVector2D(width(), height()));
            glass->setUniformValue("frameRect", vector(window.rect));
            glass->setUniformValue("contentRect", vector(content(window)));
            glass->setUniformValue("buttonCount", 3);
            glass->setUniformValue("buttonCloseIndex", 2);
            for (int button = 0; button < 3; ++button) {
                const QByteArray name = QByteArray("buttonRect[") + QByteArray::number(button) + ']';
                glass->setUniformValue(name.constData(), vector(buttonRect(window, button)));
            }
            glass->setUniformValue("accent", window.color);
            glass->setUniformValue("pointer", QVector2D(pointer));
            glass->setUniformValue("refraction", refraction);
            glass->setUniformValue("tint", tint);
            glass->setUniformValue("frameOpacity", 0.84f);
            glass->setUniformValue("dispersion", dispersion);
            glass->setUniformValue("specularStrength", 1.0f);
            glass->setUniformValue("hoverGlowStrength", 1.0f);
            glass->setUniformValue("adaptiveContrast", 0.35f);
            glass->setUniformValue("frameRadius", 5.0f);
            glass->setUniformValue("titleHeight", 39.0f);
            glass->setUniformValue("scale", float(dpr));
            glass->setUniformValue("hoverButton", hitWindow == index ? hitButton : -1);
            glass->setUniformValue("pressedButton", pressedWindow == index ? pressedButton : -1);
            triangle();
            glass->release();

            // Text and client content enter the scene after their own glass pass.
            // The next window can refract them, without capturing its own title.
            QOpenGLPaintDevice device(pixels);
            device.setDevicePixelRatio(dpr);
            QPainter painter(&device);
            paintClient(painter, window);
            paintLabels(painter, window, index);
            painter.end();
            current = next;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, buffers[current]->handle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
        glBlitFramebuffer(0, 0, pixels.width(), pixels.height(), 0, 0, pixels.width(), pixels.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        pointer = event->position();
        if (dragged >= 0) {
            QPointF pos = pointer - dragOffset;
            pos.setX(qBound(8.0, pos.x(), width() - windows[dragged].rect.width() - 8.0));
            pos.setY(qBound(8.0, pos.y(), height() - windows[dragged].rect.height() - 8.0));
            windows[dragged].rect.moveTopLeft(pos);
        }
        const auto [window, button] = hit(pointer);
        setCursor(button >= 0 ? Qt::PointingHandCursor : (window >= 0 && pointer.y() < windows[window].rect.y() + 39 ? Qt::OpenHandCursor : Qt::ArrowCursor));
        update();
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton) return;
        pointer = event->position();
        const auto [window, button] = hit(pointer);
        pressedWindow = window;
        pressedButton = button;
        if (window >= 0 && button < 0 && pointer.y() < windows[window].rect.y() + 39) {
            dragged = window;
            dragOffset = pointer - windows[window].rect.topLeft();
        }
        update();
    }

    void mouseReleaseEvent(QMouseEvent *) override
    {
        dragged = -1;
        pressedWindow = pressedButton = -1;
        update();
    }

    void leaveEvent(QEvent *) override
    {
        pointer = QPointF(-100, -100);
        update();
    }

private:
    std::unique_ptr<QOpenGLShaderProgram> program(const QString &fragment)
    {
        auto result = std::make_unique<QOpenGLShaderProgram>();
        const QString dir = QStringLiteral(PREVIEW_SHADER_DIR) + '/';
        if (!result->addShaderFromSourceFile(QOpenGLShader::Vertex, dir + "fullscreen.vert") ||
            !result->addShaderFromSourceFile(QOpenGLShader::Fragment, dir + fragment) || !result->link()) {
            qFatal("Shader failed: %s", qPrintable(result->log()));
        }
        return result;
    }

    void prepare(QSize size)
    {
        glViewport(0, 0, size.width(), size.height());
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glBindVertexArray(vao);
    }

    void triangle() { glDrawArrays(GL_TRIANGLES, 0, 3); }
    static QVector4D vector(QRectF r) { return {float(r.x()), float(r.y()), float(r.width()), float(r.height())}; }
    static QRectF content(const GlassWindow &window) { return window.rect.adjusted(8, 39, -8, -8); }
    static QRectF buttonRect(const GlassWindow &w, int index) { return {w.rect.right() - 104 + index * 30, w.rect.top() + 8, 27, 23}; }

    std::pair<int, int> hit(QPointF pos) const
    {
        for (int index = int(windows.size()) - 1; index >= 0; --index) {
            if (!windows[index].rect.contains(pos)) continue;
            for (int button = 0; button < 3; ++button) {
                if (buttonRect(windows[index], button).contains(pos)) return {index, button};
            }
            return {index, -1};
        }
        return {-1, -1};
    }

    void paintClient(QPainter &p, const GlassWindow &window)
    {
        const QRectF area = content(window);
        p.save();
        p.setClipRect(area);
        p.fillRect(area, window.terminal ? QColor("#101b22") : QColor("#e4e9e8"));
        p.setFont(QFont(window.terminal ? "monospace" : "sans-serif", 10));
        if (window.terminal) {
            p.setPen(QColor("#9dc9b6"));
            p.drawText(area.adjusted(20, 16, -16, -16), Qt::AlignLeft | Qt::AlignTop,
                       "~/studio\n\n  emerald / ruby\n  refraction      enabled\n  inner bevel     enabled\n\n  Polished edge.\n  Transparency.\n  Depth.");
        } else {
            p.fillRect(QRectF(area.x(), area.y(), 126, area.height()), QColor("#cbd6d3"));
            p.setPen(QColor("#405c51"));
            p.drawText(area.adjusted(18, 22, -18, -18), Qt::AlignLeft | Qt::AlignTop, "Library\n\nFavorites\nPictures\nMusic\nDocuments");
            QRectF page = area.adjusted(148, 22, -22, -22);
            p.setPen(QColor("#243a34"));
            p.setFont(QFont("sans-serif", 17));
            p.drawText(page, Qt::AlignLeft | Qt::AlignTop, "Light, Color, Glass");
            p.setFont(QFont("sans-serif", 10));
            p.drawText(page.adjusted(0, 44, 0, 0), Qt::AlignLeft | Qt::AlignTop, "Crisp edge. Live backdrop.\n\nEmerald and ruby glass material\nwith dual optical bevels.");
            for (int i = 0; i < 3; ++i) {
                QColor color = QColor::fromRgbF(0.11 + i * 0.15, 0.44 - i * 0.11, 0.30 - i * 0.07);
                p.fillRect(QRectF(page.left() + i * 78, page.top() + 155, 65, 65), color);
            }
        }
        p.restore();
    }

    void paintLabels(QPainter &p, const GlassWindow &window, int index)
    {
        p.setRenderHint(QPainter::Antialiasing);
        p.setFont(QFont("sans-serif", 10, QFont::DemiBold));
        QRectF title(window.rect.x() + 17, window.rect.y(), window.rect.width() - 134, 38);
        p.setPen(QColor(0, 0, 0, 205));
        p.drawText(title.translated(0, 1), Qt::AlignVCenter, window.title);
        p.setPen(QColor("#effff6"));
        p.drawText(title, Qt::AlignVCenter, window.title);
        p.setPen(QPen(QColor("#effff6"), 1.4));
        for (int button = 0; button < 3; ++button) {
            QPointF c = buttonRect(window, button).center();
            if (pressedWindow == index && pressedButton == button) c.ry() += 1;
            if (button == 0) p.drawLine(c + QPointF(-4, 3), c + QPointF(4, 3));
            if (button == 1) p.drawRect(QRectF(c - QPointF(4, 3.5), QSizeF(8, 7)));
            if (button == 2) {
                p.drawLine(c + QPointF(-3, -3), c + QPointF(3, 3));
                p.drawLine(c + QPointF(-3, 3), c + QPointF(3, -3));
            }
        }
    }

    std::array<GlassWindow, 2> windows;
    std::array<std::unique_ptr<QOpenGLFramebufferObject>, 2> buffers;
    std::unique_ptr<QOpenGLShaderProgram> background, glass;
    GLuint vao = 0;
    QTimer timer;
    QElapsedTimer elapsed;
    QPointF pointer{-100, -100}, dragOffset;
    int dragged = -1, pressedWindow = -1, pressedButton = -1;
    float refraction = 0.85f, tint = 0.7f, dispersion = 0.25f, frozenTime = 0;
    bool light = false;
};

int main(int argc, char **argv)
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
#ifdef HAS_MATERIAL_PREVIEW
    if (app.arguments().contains("--close-comparison")) return runCloseComparison(app);
    if (app.arguments().contains("--sheet-comparison")) return runSheetComparison(app);
    if (app.arguments().contains("--kcm-preview")) {
        auto result = KPluginFactory::instantiatePlugin<KCModule>(KPluginMetaData(QStringLiteral(KCM_PLUGIN_PATH)));
        if (!result) return 1;
        std::unique_ptr<KCModule> module(result.plugin);
        module->load();
        QWidget host;
        auto *layout = new QVBoxLayout(&host);
        layout->addWidget(new QLabel(QStringLiteral("Actual decoration configuration module")));
        layout->addWidget(module->widget());
        host.resize(900, 630);
        host.show();
        module->widget()->findChild<QTabWidget *>()->setCurrentIndex(1);
        const auto args = app.arguments();
        const int blurArgument = args.indexOf("--blur");
        if (blurArgument >= 0 && blurArgument + 1 < args.size())
            module->widget()->findChild<QSlider *>(QStringLiteral("frameBlurStrength"))->setValue(args[blurArgument + 1].toInt());
        if (args.contains("--verify-preview")) {
            QTimer::singleShot(1500, &host, [&] {
                module->widget()->findChild<QCheckBox *>(QStringLiteral("useLiquidGlass"))->setChecked(true);
                auto *refraction = module->widget()->findChild<QSlider *>(QStringLiteral("refractionStrength"));
                auto *dispersion = module->widget()->findChild<QSlider *>(QStringLiteral("chromaticDispersion"));
                auto *sample = module->widget()->findChild<QWidget *>(QStringLiteral("materialPreview"));
                auto captureSample = [&] {
                    QApplication::processEvents();
                    return sample->grab().toImage();
                };
                refraction->setValue(0); dispersion->setValue(0); const QImage flat = captureSample();
                refraction->setValue(180); const QImage lens = captureSample();
                refraction->setValue(78); const QImage ordinaryLens = captureSample();
                dispersion->setValue(100); const QImage color = captureSample();
                refraction->setValue(0); const QImage reset = captureSample();
                // Caption body of the active sample, excluding its text, buttons and outer contour.
                const qreal dpr = flat.devicePixelRatio();
                const QRect region(qRound(28*dpr), qRound(30*dpr), qRound((sample->width()/2-150)*dpr), qRound(14*dpr));
                auto difference = [&](const QImage &a, const QImage &b, const QRect &area = QRect()) {
                    const QRect compared = area.isEmpty() ? region : area;
                    double sum = 0;
                    for (int y=compared.top(); y<=compared.bottom(); ++y) for (int x=compared.left(); x<=compared.right(); ++x) {
                        const auto ca=a.pixelColor(x,y), cb=b.pixelColor(x,y);
                        sum += std::abs(ca.red()-cb.red())+std::abs(ca.green()-cb.green())+std::abs(ca.blue()-cb.blue());
                    }
                    return sum/(compared.width()*compared.height()*3);
                };
                const QRect opticsRegion(region.x(),qRound(12*dpr),region.width(),qRound(6*dpr));
                const double refDelta=difference(flat,lens,opticsRegion), dispDelta=difference(ordinaryLens,color,opticsRegion);
                std::fprintf(stderr,"KCM slider -> rim RGB mean difference: refraction=%.3f dispersion=%.3f reset=%.3f; controls enabled=%d/%d\n",
                    refDelta,dispDelta,difference(flat,reset),refraction->isEnabled(),dispersion->isEnabled());
                flat.save(QStringLiteral("/tmp/emerald-kcm-flat.png"));
                lens.save(QStringLiteral("/tmp/emerald-kcm-lens.png"));
                color.save(QStringLiteral("/tmp/emerald-kcm-dispersion.png"));
                const bool backdropStable = flat.copy(0,0,flat.width(),qRound(8*dpr)) == color.copy(0,0,color.width(),qRound(8*dpr));
                const QRect client(qRound(28*dpr),qRound(70*dpr),qRound(100*dpr),qRound(60*dpr));
                const bool clientStable = flat.copy(client) == color.copy(client);
                auto *adaptive = module->widget()->findChild<QSlider *>(QStringLiteral("adaptiveBackdropContrast"));
                const int savedContrast = adaptive->value();
                adaptive->setValue(0); const QImage noContrast = captureSample();
                adaptive->setValue(100); const QImage readable = captureSample();
                const QRect brightRegion = region.translated(qRound(sample->width()*0.5*dpr),0);
                const double adaptiveDelta = difference(noContrast,readable,brightRegion);
                const bool adaptiveWorks = adaptiveDelta>2;
                std::fprintf(stderr,"KCM adaptive contrast: header RGB difference=%.3f\n",adaptiveDelta);
                noContrast.save(QStringLiteral("/tmp/emerald-contrast-off.png"));
                readable.save(QStringLiteral("/tmp/emerald-contrast-on.png"));
                adaptive->setValue(savedContrast);
                auto *useOptics = module->widget()->findChild<QCheckBox *>(QStringLiteral("useLiquidGlass"));
                auto *blur = module->widget()->findChild<QSlider *>(QStringLiteral("frameBlurStrength"));
                const int savedBlur = blur->value();
                useOptics->setChecked(false);
                blur->setValue(0); const QImage noBlur = captureSample();
                blur->setValue(15); const QImage blurred = captureSample();
                const bool independentBlur = blur->isEnabled() && !refraction->isEnabled() && !dispersion->isEnabled()
                    && difference(noBlur,blurred)>1;
                std::fprintf(stderr,"KCM optics OFF: blur enabled=%d, optics enabled=%d, header blur RGB difference=%.3f\n",
                    blur->isEnabled(),refraction->isEnabled(),difference(noBlur,blurred));
                blur->setValue(savedBlur); useOptics->setChecked(true);
                refraction->setValue(180);
                host.grab().save(QStringLiteral("/tmp/emerald-kcm-window.png"));
                host.resize(720,650); QApplication::processEvents();
                const bool resized = !captureSample().isNull();
                app.exit(refraction->isEnabled() && dispersion->isEnabled() && refDelta>1 && dispDelta>1 && flat==reset && backdropStable && clientStable && independentBlur && adaptiveWorks && resized ? 0 : 1);
            });
        }
        const int capture = args.indexOf("--capture");
        if (capture >= 0 && capture + 1 < args.size()) {
            QTimer::singleShot(1500, &host, [&] {
                const bool saved = host.grab().save(args[capture + 1]);
                app.exit(saved ? 0 : 1);
            });
        }
        return app.exec();
    }
    if (app.arguments().contains("--material-preview")) {
        EmeraldGlass::MaterialPreview preview;
        auto cfg = EmeraldGlass::loadMaterialConfig();
        cfg.shaderEffectEnabled = true;
        const auto args = app.arguments();
        auto value = [&](const char *name, int fallback) {
            const int index = args.indexOf(QString::fromLatin1(name));
            return index >= 0 && index + 1 < args.size() ? args[index + 1].toInt() : fallback;
        };
        cfg.refraction = value("--refraction", qRound(cfg.refraction * 100)) / 100.0f;
        cfg.dispersion = value("--dispersion", qRound(cfg.dispersion * 100)) / 100.0f;
        cfg.blurStrength = value("--blur", cfg.blurStrength);
        if (args.contains("--verify-preview")) {
            // Measure optics with transmission enabled, independently of saved opacity.
            cfg.frameOpacity = .35f;
            cfg.tint = cfg.inactiveTint = .35f;
            cfg.blurStrength = value("--blur", 2);
        }
        preview.setConfig(cfg);
        preview.setEffectAvailable(true);
        preview.setWindowTitle(QStringLiteral("Emerald Glass — Settings Material Preview"));
        preview.resize(900, 210);
        preview.show();
        if (args.contains("--verify-preview")) {
            QTimer::singleShot(250, &preview, [&] {
                cfg.refraction = 0; cfg.dispersion = 0; preview.setConfig(cfg);
                const QImage flat = preview.grabFramebuffer();
                cfg.refraction = 1.8f; preview.setConfig(cfg);
                const QImage refracted = preview.grabFramebuffer();
                cfg.refraction = .78f; preview.setConfig(cfg);
                const QImage ordinaryLens = preview.grabFramebuffer();
                cfg.dispersion = 1.0f; preview.setConfig(cfg);
                const QImage dispersed = preview.grabFramebuffer();
                cfg.refraction = 0; preview.setConfig(cfg);
                const QImage reset = preview.grabFramebuffer();
                auto difference = [](const QImage &a, const QImage &b) {
                    qint64 total = 0;
                    for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x) {
                        const QColor ca = a.pixelColor(x, y), cb = b.pixelColor(x, y);
                        total += std::abs(ca.red() - cb.red()) + std::abs(ca.green() - cb.green()) + std::abs(ca.blue() - cb.blue());
                    }
                    return total;
                };
                const auto refractionDelta = difference(flat, refracted);
                const auto dispersionDelta = difference(ordinaryLens, dispersed);
                std::fprintf(stderr, "Settings preview immediate changes: refraction=%lld dispersion=%lld reset=%lld\n",
                    static_cast<long long>(refractionDelta), static_cast<long long>(dispersionDelta),
                    static_cast<long long>(difference(flat, reset)));
                app.exit(!flat.isNull() && refractionDelta > 50000 && dispersionDelta > 15000 && flat == reset ? 0 : 1);
            });
            return app.exec();
        }
        const int capture = args.indexOf("--capture");
        if (capture >= 0 && capture + 1 < args.size()) {
            QTimer::singleShot(250, &preview, [&] {
                const bool saved = preview.grabFramebuffer().save(args[capture + 1]);
                app.exit(saved ? 0 : 1);
            });
        }
        return app.exec();
    }
#endif
    QWidget window;
    window.setWindowTitle("Emerald Glass — Shader Prototype");
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 12, 0, 0);
    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(18, 0, 18, 0);
    auto *preview = new Preview;
    controls->addWidget(new QLabel("Refraction"));
    auto *refract = new QSlider(Qt::Horizontal);
    refract->setRange(0, 180);
    refract->setValue(85);
    controls->addWidget(refract);
    controls->addSpacing(16);
    controls->addWidget(new QLabel("Dispersion"));
    auto *disp = new QSlider(Qt::Horizontal);
    disp->setRange(0, 100);
    disp->setValue(25);
    controls->addWidget(disp);
    controls->addSpacing(16);
    controls->addWidget(new QLabel("Glass Tint"));
    auto *tint = new QSlider(Qt::Horizontal);
    tint->setRange(0, 200);
    tint->setValue(70);
    controls->addWidget(tint);
    auto *light = new QCheckBox("Light Backdrop");
    controls->addWidget(light);
    auto *animate = new QCheckBox("Animate Backdrop");
    animate->setChecked(true);
    controls->addWidget(animate);
    layout->addLayout(controls);
    auto *hint = new QLabel("Drag windows by titlebar · Hover and press buttons to inspect glass material");
    hint->setContentsMargins(18, 2, 18, 5);
    layout->addWidget(hint);
    layout->addWidget(preview, 1);
    QObject::connect(refract, &QSlider::valueChanged, preview, &Preview::setRefraction);
    QObject::connect(disp, &QSlider::valueChanged, preview, &Preview::setDispersion);
    QObject::connect(tint, &QSlider::valueChanged, preview, &Preview::setTint);
    QObject::connect(light, &QCheckBox::toggled, preview, &Preview::setLight);
    QObject::connect(animate, &QCheckBox::toggled, preview, &Preview::setAnimation);
    window.resize(1100, 740);
    window.show();

    const QStringList arguments = app.arguments();
    if (arguments.contains("--light")) light->setChecked(true);
    const int capture = arguments.indexOf("--capture");
    if (capture >= 0 && capture + 1 < arguments.size()) {
        animate->setChecked(false);
        QTimer::singleShot(800, preview, [&, path = arguments[capture + 1]] {
            const bool saved = preview->grabFramebuffer().save(path);
            std::fprintf(stderr, "Capture saved: %s (%s)\n", qPrintable(path), saved ? "ok" : "failed");
            app.exit(saved ? 0 : 1);
        });
    }
    return app.exec();
}
