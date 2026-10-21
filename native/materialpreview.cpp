// SPDX-License-Identifier: LGPL-3.0-or-later
#include "materialpreview.h"
#include "blurprofile.h"

#include <QFile>
#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QRadialGradient>
#include <QSurfaceFormat>
#include <QScopeGuard>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

static void initializeMaterialResources()
{
    Q_INIT_RESOURCE(materialpreview);
}

namespace EmeraldGlass {

MaterialPreview::MaterialPreview(QWidget *parent, const QString &shaderPath, int closePreviewStyle)
    : QWidget(parent)
    , m_shaderPath(shaderPath.isEmpty() ? QStringLiteral(":/emeraldglass/preview/glass.frag") : shaderPath)
    , m_closePreviewStyle(closePreviewStyle)
{
    initializeMaterialResources();
    setMinimumHeight(180);
    setMouseTracking(true);
    setObjectName(QStringLiteral("materialPreview"));
}

MaterialPreview::~MaterialPreview()
{
    auto *previous = QOpenGLContext::currentContext();
    auto *surface = previous ? previous->surface() : nullptr;
    if (m_context.isValid() && m_context.makeCurrent(&m_surface)) {
        m_output.reset();
        m_buffers.clear();
        m_material.reset();
        m_downsample.reset();
        m_upsample.reset();
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_context.doneCurrent();
        if (previous && previous != &m_context && surface) previous->makeCurrent(surface);
    }
}

void MaterialPreview::setConfig(const MaterialConfig &cfg)
{
    m_cfg = cfg;
    update();
}

void MaterialPreview::setClosePreviewState(int state)
{
    m_closePreviewState = state;
    update();
}

void MaterialPreview::setEffectAvailable(bool available)
{
    if (m_effectAvailable != available) {
        m_effectAvailable = available;
        update();
    }
}

std::unique_ptr<QOpenGLShaderProgram> MaterialPreview::program(const char *path, bool preamble)
{
    auto result = std::make_unique<QOpenGLShaderProgram>();
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray fragment = (preamble ? QByteArray("#version 330 core\nout vec4 fragColor;\n") : QByteArray()) + file.readAll();
    if (!preamble && m_closePreviewStyle >= 0) {
        fragment.insert(fragment.indexOf('\n') + 1, QByteArray("#define CLOSE_PREVIEW_STYLE ") + QByteArray::number(m_closePreviewStyle) + '\n');
    }
    if (!result->addShaderFromSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/emeraldglass/preview/fullscreen.vert")) ||
        !result->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment) || !result->link()) {
        qWarning() << "Emerald Glass preview shader:" << result->log();
        return {};
    }
    return result;
}

bool MaterialPreview::initializeGL()
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    m_context.setFormat(format);
    if (!m_context.create()) return false;
    m_surface.setFormat(m_context.format());
    m_surface.create();
    if (!m_context.makeCurrent(&m_surface) || !initializeOpenGLFunctions()) return false;
    glGenVertexArrays(1, &m_vao);
    m_material = program(m_shaderPath.toUtf8().constData());
    m_downsample = program(":/emeraldglass/preview/downsample.frag", true);
    m_upsample = program(":/emeraldglass/preview/upsample.frag", true);
    return m_material && m_downsample && m_upsample;
}

QRectF MaterialPreview::content(const QRectF &frame) const
{
    return frame.adjusted(m_cfg.borderWidth, m_cfg.titleHeight, -m_cfg.borderWidth, -m_cfg.borderWidth);
}

PreviewFrame MaterialPreview::sampleFrame(const QRectF &box, bool active) const
{
    PreviewFrame sample;
    sample.frame = box; sample.content = content(box); sample.active = active;
    sample.radius = m_cfg.cornerRadius; sample.pointer = m_pointer;
    const KConfigGroup settings(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("org.kde.kdecoration2"));
    auto visible = [&](QString types) {
        QString result;
        for (QChar type : types) {
            if (!QStringLiteral("MNSIAXFBE_").contains(type)) continue;
            if (type == QLatin1Char('E') && !settings.readEntry("AlwaysShowExcludeFromCapture", false)) continue;
            result += type;
        }
        return result;
    };
    const QString left = m_cfg.useSystemButtons ? visible(settings.readEntry("ButtonsOnLeft", QStringLiteral("MS"))) : QString();
    const QString right = m_cfg.useSystemButtons ? visible(settings.readEntry("ButtonsOnRight", QStringLiteral("HIAX"))) : QStringLiteral("IAX");
    auto groupWidth = [](const QString &types) {
        qreal width = 0;
        for (QChar type : types) width += (type == QLatin1Char('_') ? 10 : 27) + 3;
        return std::max(0.0, width - 3);
    };
    auto append = [&](const QString &types, qreal x) {
        const qreal y = box.y() + std::max(2.0, std::floor((m_cfg.titleHeight - 23.0) * 0.5));
        for (QChar type : types) {
            if (type != QLatin1Char('_')) {
                if (type == QLatin1Char('X')) sample.closeButton = sample.buttons.size();
                sample.buttons.append(QRectF(x, y, 27, 23));
                sample.buttonTypes += type;
                if (sample.buttons.last().contains(m_pointer)) sample.hoverButton = sample.buttons.size() - 1;
            }
            x += (type == QLatin1Char('_') ? 10 : 27) + 3;
        }
    };
    append(left, box.x() + 14);
    append(right, std::max(box.x() + 14, box.right() - groupWidth(right) - 17));
    if (m_closePreviewState >= 0 && sample.closeButton >= 0) {
        sample.hoverButton = m_closePreviewState == 1 ? sample.closeButton : -1;
        sample.pressedButton = m_closePreviewState == 2 ? sample.closeButton : -1;
        sample.pointer = m_closePreviewState > 0 ? sample.buttons[sample.closeButton].center() : QPointF(-100, -100);
    }
    const qreal leftInset = left.isEmpty() ? 17 : 14 + groupWidth(left) + 10;
    const qreal rightInset = right.isEmpty() ? 17 : 17 + groupWidth(right) + 10;
    sample.titleRect = box.adjusted(leftInset, 0, -rightInset, 0);
    sample.titleRect.setHeight(m_cfg.titleHeight - 1);
    return sample;
}

void MaterialPreview::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.drawImage(QPoint(), grabFramebuffer());
}

QImage MaterialPreview::grabFramebuffer()
{
    return renderImage(size(), devicePixelRatioF(), nullptr);
}

QImage MaterialPreview::renderDecorationFrame(const QSizeF &size, qreal dpr, const PreviewFrame &frame)
{
    return renderImage(size, dpr, &frame);
}

QImage MaterialPreview::renderImage(const QSizeF &size, qreal dpr, const PreviewFrame *frame)
{
    if (size.isEmpty()) return {};
    // KCMs may be embedded in a Qt Quick scene with its own GL context and FBO.
    // Never render into, or leave GL state in, the host's context.
    auto *previous = QOpenGLContext::currentContext();
    auto *surface = previous ? previous->surface() : nullptr;
    const auto restore = qScopeGuard([&] {
        m_context.doneCurrent();
        if (previous && surface) previous->makeCurrent(surface);
    });
    if (!m_initialized) {
        m_initialized = true;
        m_ready = initializeGL();
    }
    if (!m_ready) {
        // A card delegates fallback to its native decoration. A failed shader
        // must never turn a demonstration backdrop into a "ready" material.
        if (frame) return {};
        QImage image((size * dpr).toSize(), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(palette().window().color());
        QPainter painter(&image);
        const qreal sampleWidth = (size.width() - 36.0) * 0.5;
        for (int i = 0; i < 2; ++i)
            drawSample(painter, QRectF(12 + i * (12 + sampleWidth), 10, sampleWidth, size.height() - 20), i == 0, false);
        return image;
    }
    if (!m_vao || !m_context.makeCurrent(&m_surface)) return {};
    return render(size, dpr, frame);
}

QImage MaterialPreview::render(const QSizeF &size, qreal dpr, const PreviewFrame *singleFrame)
{
    const qreal width = size.width(), height = size.height();
    const QSize pixels(qRound(width * dpr), qRound(height * dpr));
    const bool gpu = m_effectAvailable && m_material && m_downsample && m_upsample;
    const auto blur = blurProfile(gpu ? m_cfg.blurStrength : 0);
    if (m_buffers.size() != size_t(blur.iterations + 1) || m_buffers.front()->size() != pixels) {
        m_buffers.clear();
        for (int i = 0; i <= blur.iterations; ++i) {
            auto buffer = std::make_unique<QOpenGLFramebufferObject>((pixels / (1 << i)).expandedTo(QSize(1, 1)));
            if (!buffer->isValid()) return {};
            glBindTexture(GL_TEXTURE_2D, buffer->texture());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            m_buffers.push_back(std::move(buffer));
        }
    }

    if (!m_output || m_output->size() != pixels)
        m_output = std::make_unique<QOpenGLFramebufferObject>(pixels);
    if (!m_output->isValid()) return {};

    QImage backdrop(pixels, QImage::Format_ARGB32_Premultiplied);
    backdrop.setDevicePixelRatio(dpr);
    {
        QPainter p(&backdrop);
        QLinearGradient bg(0, 0, width, height);
        bg.setColorAt(0.0, QColor("#123943"));
        bg.setColorAt(0.5, QColor("#617d88"));
        bg.setColorAt(1.0, QColor("#e7d9b8"));
        p.fillRect(QRectF(QPointF(), size), bg);
        // Fine lines and broad color transitions reveal lensing at realistic blur levels.
        p.setPen(QPen(QColor(235, 250, 255, 160), 4));
        for (int x = -height; x < width; x += 32) {
            p.drawLine(QPoint(x, 0), QPoint(x + height, height));
        }
        for (int y = 24; y < height; y += 48) {
            p.fillRect(QRect(0, y, width, 12), QColor(5, 14, 24, 190));
        }
        p.setPen(QPen(QColor(245, 175, 70, 180), 12));
        p.drawLine(QPointF(width * 0.15, 0), QPointF(width * 0.45, height));
        p.setPen(QPen(QColor(40, 130, 245, 190), 14));
        p.drawLine(QPointF(width * 0.8, 0), QPointF(width * 0.6, height));
    }

    const QImage texture = backdrop.convertToFormat(QImage::Format_RGBA8888).flipped(Qt::Vertical);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_buffers[0]->texture());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pixels.width(), pixels.height(), GL_RGBA, GL_UNSIGNED_BYTE, texture.constBits());

    const KConfigGroup settings(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-emeraldglass"));
    const KConfigGroup standardBlur(KSharedConfig::openConfig(QStringLiteral("kwinrc")), QStringLiteral("Effect-blur"));
    const float backdropSaturation = std::max(0, standardBlur.readEntry("Saturation", 100)) / 100.0f;
    const float blurScale = 0.5f + std::clamp(settings.readEntry("BlurFinetune", 3), 0, 10) * 0.13f;
    auto pass = [&](QOpenGLShaderProgram &shader, int source, int destination) {
        m_buffers[destination]->bind();
        const QSize size = m_buffers[destination]->size();
        glViewport(0, 0, size.width(), size.height());
        glDisable(GL_BLEND);
        shader.bind();
        shader.setUniformValue("texUnit", 0);
        shader.setUniformValue("offset", blur.offset * blurScale);
        shader.setUniformValue("halfpixel", QVector2D(0.5f / m_buffers[source]->width(), 0.5f / m_buffers[source]->height()));
        if (&shader == m_upsample.get()) shader.setUniformValue("saturationCompensation", 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_buffers[source]->texture());
        glBindVertexArray(m_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        shader.release();
    };
    for (int i = 1; i <= blur.iterations; ++i) pass(*m_downsample, i - 1, i);
    for (int i = blur.iterations; i > 1; --i) pass(*m_upsample, i, i - 1);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_buffers[0]->handle());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_output->handle());
    glBlitFramebuffer(0, 0, pixels.width(), pixels.height(), 0, 0, pixels.width(), pixels.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, m_output->handle());
    glViewport(0, 0, pixels.width(), pixels.height());
    const qreal sampleW = (width - 36.0) * 0.5;
    QList<PreviewFrame> frames;
    if (singleFrame) {
        frames.append(*singleFrame);
    } else {
        for (int i = 0; i < 2; ++i) {
            frames.append(sampleFrame(QRectF(12 + i * (12 + sampleW), 10, sampleW, height - 20.0), i == 0));
        }
    }
    if (gpu) {
        auto rectUniform = [](const QRectF &r) { return QVector4D(r.x(), r.y(), r.width(), r.height()); };
        m_material->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_buffers[blur.iterations > 0 ? 1 : 0]->texture());
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        for (const auto &sample : frames) {
            m_material->setUniformValue("kwinMode", 1);
            m_material->setUniformValue("opticsDisabled", m_cfg.shaderEffectEnabled ? 0 : 1);
            m_material->setUniformValue("backdropSaturationOffset", backdropSaturation - 1.0f);
            m_material->setUniformValue("scene", 0);
            m_material->setUniformValue("resolution", QVector2D(width, height));
            m_material->setUniformValue("frameRect", rectUniform(sample.frame));
            m_material->setUniformValue("contentRect", rectUniform(sample.content));
            m_material->setUniformValue("buttonCount", std::min(12, int(sample.buttons.size())));
            m_material->setUniformValue("buttonCloseIndex", sample.closeButton);
            for (int j = 0; j < std::min(12, int(sample.buttons.size())); ++j) {
                const QByteArray name = QByteArray("buttonRect[") + QByteArray::number(j) + ']';
                m_material->setUniformValue(name.constData(), rectUniform(sample.buttons[j]));
            }
            const QColor accent = m_cfg.colorForWindow(sample.active);
            m_material->setUniformValue("accent", QVector3D(accent.redF(), accent.greenF(), accent.blueF()));
            m_material->setUniformValue("pointer", QVector2D(sample.pointer));
            m_material->setUniformValue("refraction", m_cfg.refraction);
            m_material->setUniformValue("dispersion", m_cfg.dispersion);
            m_material->setUniformValue("tint", m_cfg.tintForWindow(sample.active));
            m_material->setUniformValue("frameOpacity", m_cfg.frameOpacity);
            m_material->setUniformValue("backgroundBlurOffset", blur.offset * blurScale);
            m_material->setUniformValue("specularStrength", m_cfg.specular);
            m_material->setUniformValue("adaptiveContrast", m_cfg.adaptiveContrast);
            m_material->setUniformValue("hoverGlowStrength", m_cfg.hoverGlow);
            m_material->setUniformValue("frameRadius", sample.radius);
            m_material->setUniformValue("titleHeight", float(m_cfg.titleHeight));
            m_material->setUniformValue("scale", float(dpr));
            m_material->setUniformValue("hoverButton", sample.hoverButton);
            m_material->setUniformValue("pressedButton", sample.pressedButton);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glDisable(GL_BLEND);
        m_material->release();
    }
    QImage image = m_output->toImage();
    image.setDevicePixelRatio(dpr);
    if (singleFrame) return image;
    QPainter overlay(&image);
    drawSample(overlay, frames[0].frame, true, gpu && m_cfg.shaderEffectEnabled);
    drawSample(overlay, frames[1].frame, false, gpu && m_cfg.shaderEffectEnabled);
    overlay.end();
    return image;
}

void MaterialPreview::drawSample(QPainter &p, const QRectF &box, bool active, bool gpu)
{
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF client = content(box);
    p.fillRect(client, active ? QColor("#14262d") : QColor("#283037"));
    const qreal dpr = devicePixelRatioF();
    const QRectF localFrame(QPointF(), box.size());
    if (!gpu) {
        QImage image = renderFrameGlassImage((box.size() * dpr).toSize(), dpr, localFrame,
            content(localFrame), m_cfg.colorForWindow(active), m_cfg.tintForWindow(active), m_cfg.cornerRadius, m_cfg);
        image.setDevicePixelRatio(dpr);
        p.drawImage(box.topLeft(), image);
    }
    if (box.contains(m_pointer) && m_cfg.hoverGlow > 0) {
        QPainterPath frame, hole;
        frame.addRoundedRect(box, m_cfg.cornerRadius, m_cfg.cornerRadius);
        hole.addRect(client);
        QRadialGradient glow(m_pointer, 60);
        QColor color = m_cfg.colorForWindow(active).lighter(135);
        color.setAlphaF(std::clamp(m_cfg.hoverGlow * 0.28f, 0.0f, 0.55f));
        glow.setColorAt(0, color); color.setAlphaF(0); glow.setColorAt(1, color);
        p.fillPath(frame.subtracted(hole), glow);
    }
    const auto sample = sampleFrame(box, active);
    for (int i = 0; i < sample.buttons.size(); ++i) {
        const QRectF b = sample.buttons[i];
        const QChar type = sample.buttonTypes[i];
        const bool hovered = b.contains(m_pointer);
        if (!gpu) paintButtonGlass(&p, b, dpr, m_cfg.colorForWindow(active), m_cfg.tintForWindow(active), i == sample.closeButton, hovered, false, m_pointer, m_cfg);
        const QPointF c = b.center() + QPointF(0, i == sample.pressedButton ? 1 : 0);
        p.setPen(QPen(QColor("#effff6"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if (type == QLatin1Char('I')) p.drawLine(c + QPointF(-4, 3), c + QPointF(4, 3));
        else if (type == QLatin1Char('A')) p.drawRect(QRectF(c - QPointF(4, 3.5), QSizeF(8, 7)));
        else if (type == QLatin1Char('X')) { p.drawLine(c + QPointF(-3, -3), c + QPointF(3, 3)); p.drawLine(c + QPointF(-3, 3), c + QPointF(3, -3)); }
        else if (type == QLatin1Char('N')) for (int y : {-3, 0, 3}) p.drawLine(c + QPointF(-4, y), c + QPointF(4, y));
        else if (type == QLatin1Char('F') || type == QLatin1Char('B')) {
            const int sign = type == QLatin1Char('F') ? 1 : -1;
            p.drawLine(c + QPointF(-4, sign * 2), c + QPointF(0, -sign * 2.5));
            p.drawLine(c + QPointF(0, -sign * 2.5), c + QPointF(4, sign * 2));
        } else p.drawEllipse(c, 3.2, 3.2);
    }
    QFont f = font(); f.setWeight(QFont::DemiBold); p.setFont(f);
    QRectF title = sample.titleRect;
    const QString text = p.fontMetrics().elidedText(active ? QStringLiteral("Active Window") : QStringLiteral("Inactive Window"), Qt::ElideRight, int(title.width()));
    if (m_cfg.titleAlignment == 2) {
        const qreal textWidth = p.fontMetrics().horizontalAdvance(text);
        const qreal centered = box.center().x() - textWidth * 0.5;
        if (centered >= title.left() && centered + textWidth <= title.right())
            title = QRectF(box.x(), box.y(), box.width(), m_cfg.titleHeight - 1);
    }
    const auto alignment = Qt::AlignVCenter | (m_cfg.titleAlignment == 0 ? Qt::AlignLeft : Qt::AlignHCenter);
    const QColor titleColor = m_cfg.titleColorForWindow(active, gpu);
    p.setPen(titleShadowColor(titleColor)); p.drawText(title.translated(0, 1), alignment, text);
    p.setPen(titleColor); p.drawText(title, alignment, text);
}

void MaterialPreview::mouseMoveEvent(QMouseEvent *event)
{
    m_pointer = event->position();
    update();
}

void MaterialPreview::leaveEvent(QEvent *)
{
    m_pointer = QPointF(-100, -100);
    update();
}

} // namespace EmeraldGlass
