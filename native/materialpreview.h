// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "material.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QVector4D>
#include <memory>
#include <vector>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

namespace EmeraldGlass {

struct PreviewFrame {
    QRectF frame;
    QRectF content;
    QList<QRectF> buttons;
    QList<QVector4D> buttonRadii;
    QList<QVector4D> buttonColors;
    QList<QVector4D> buttonColorMeta;
    QString buttonTypes;
    QRectF titleRect;
    QPointF pointer{-100.0, -100.0};
    bool active = true;
    float radius = 5.0f;
    int gradientMode = 0;
    QColor secondaryColor;
    int buttonShape = 0;
    int closeButton = -1;
    int hoverButton = -1;
    int pressedButton = -1;
    bool operator==(const PreviewFrame &) const = default;
};

class MaterialPreview : public QWidget, protected QOpenGLFunctions_3_3_Core {
public:
    explicit MaterialPreview(QWidget *parent = nullptr, const QString &shaderPath = {}, int closePreviewStyle = -1);
    ~MaterialPreview() override;
    void setConfig(const MaterialConfig &cfg);
    void setDesktopBackdrop(const QImage &image, int blurFinetune = 3);
    void setEffectAvailable(bool available);
    void setClosePreviewState(int state);
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
    void drawSample(QPainter &p, const PreviewFrame &sample, bool gpu);
    QRectF content(const QRectF &frame) const;
    PreviewFrame sampleFrame(const QRectF &frame, bool active) const;

    QImage m_desktopBackdrop;
    bool m_desktopMode = false;
    int m_desktopBlurFinetune = 3;
    QString m_shaderPath;
    int m_closePreviewStyle = -1;
    int m_closePreviewState = -1;
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
