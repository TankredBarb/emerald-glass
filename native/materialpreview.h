// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "material.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <memory>
#include <vector>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

namespace EmeraldGlass {

struct PreviewFrame {
    QRectF frame;
    QRectF content;
    QList<QRectF> buttons;
    QString buttonTypes;
    QRectF titleRect;
    QPointF pointer{-100.0, -100.0};
    bool active = true;
    float radius = 5.0f;
    int closeButton = -1;
    int hoverButton = -1;
    int pressedButton = -1;
    bool operator==(const PreviewFrame &) const = default;
};

class MaterialPreview : public QWidget, protected QOpenGLFunctions_3_3_Core {
public:
    explicit MaterialPreview(QWidget *parent = nullptr, const QString &shaderPath = {});
    ~MaterialPreview() override;
    void setConfig(const MaterialConfig &cfg);
    void setEffectAvailable(bool available);
    QImage grabFramebuffer();
    QImage renderDecorationFrame(const QSizeF &size, qreal dpr, const PreviewFrame &frame);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool initializeGL();
    QImage renderImage(const QSizeF &size, qreal dpr, const PreviewFrame *frame);
    QImage render(const QSizeF &size, qreal dpr, const PreviewFrame *frame);
    std::unique_ptr<QOpenGLShaderProgram> program(const char *path, bool preamble = false);
    void drawSample(QPainter &p, const QRectF &box, bool active, bool gpu);
    QRectF content(const QRectF &frame) const;
    PreviewFrame sampleFrame(const QRectF &frame, bool active) const;

    QString m_shaderPath;
    MaterialConfig m_cfg;
    bool m_effectAvailable = false;
    QPointF m_pointer{-100.0, -100.0};
    QOpenGLContext m_context;
    QOffscreenSurface m_surface;
    bool m_initialized = false;
    bool m_ready = false;
    GLuint m_vao = 0;
    std::unique_ptr<QOpenGLFramebufferObject> m_output;
    std::unique_ptr<QOpenGLShaderProgram> m_material;
    std::unique_ptr<QOpenGLShaderProgram> m_downsample;
    std::unique_ptr<QOpenGLShaderProgram> m_upsample;
    std::vector<std::unique_ptr<QOpenGLFramebufferObject>> m_buffers;
};

} // namespace EmeraldGlass
