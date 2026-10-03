// SPDX-FileCopyrightText: 2026 Pavel Švec
// SPDX-License-Identifier: GPL-2.0-or-later

#include "xmb_wave_renderer.h"

#include "xmb_logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImage>
#include <QMatrix4x4>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QtConcurrent>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <cmath>

namespace {

// The XMB lays out its 3D pages in PSP screen units: 480 x 272 at the z = 0 plane.
constexpr float kPspWidth = 480.f;
constexpr float kPspHeight = 272.f;
constexpr float kPspFovY = 30.f; // degrees, at PSP aspect

bool systemOnBattery()
{
    const QDir ps(QStringLiteral("/sys/class/power_supply"));
    bool sawAc = false;
    for (const QString &entry : ps.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile type(ps.filePath(entry + QStringLiteral("/type")));
        if (!type.open(QIODevice::ReadOnly) || type.readAll().trimmed() != "Mains")
            continue;
        QFile online(ps.filePath(entry + QStringLiteral("/online")));
        if (!online.open(QIODevice::ReadOnly))
            continue;
        sawAc = true;
        if (online.readAll().trimmed() == "1")
            return false;
    }
    return sawAc;
}

bool debugEnabledFromEnv()
{
    const QByteArray v = qgetenv("VLNKY_WAVE_DEBUG");
    return !v.isEmpty() && v != "0" && v != "false";
}

QShader loadShader(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

struct UniformBlock {
    float mvp[16];
    float color[4];
    float params[4]; // x = debug mode, y = bicubic, zw = 1 / texture size
};

struct PostUniform {
    float texel[4];  // xy = 1 / source size, zw = footprint or direction
    float params[4];
};

struct CompositeUniform {
    float bgTop[4];
    float bgBottom[4];
    float bgParams[4]; // mode, angle (rad), brightness, NDC y up
    float ps2Color[4]; // rgb, a = enabled
    float fx[4];       // bloom, vignette, dither, time
    float view[4];     // output w, h, wave centre, wave opacity
    float scene[4];    // 1/w, 1/h, scene present, supersampling
    float svec[8];     // Svec Studio wave parameters, see composite.frag
};

void segmentsForLevel(int level, int &u, int &v)
{
    switch (level) {
    case 1:
        u = 96;
        v = 28;
        break;
    case 3:
        u = 264;
        v = 80;
        break;
    case 4: // Ultra: smooth silhouettes on 1440p / 4K
        u = 400;
        v = 120;
        break;
    default:
        u = 168;
        v = 48;
        break;
    }
}

constexpr float kQuad[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f};

} // namespace

// --- Render thread ------------------------------------------------------------------------------
//
// Frame graph:
//   wave pass     : B-spline mesh -> scene texture (RGBA16F, MSAA resolve, optional supersampling)
//   bloom passes  : scene -> 1/4 res box downsample -> Gaussian H -> Gaussian V
//   composite     : background (gradient / XMB theme / PS2 wave) + scene + bloom, vignette,
//                   dither -> the item's own texture (premultiplied; alpha 0 = additive over QML)

class XmbWaveRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    struct Offscreen {
        QSize outSize;
        QSize sceneSize;
        QSize bloomSize;
        int samples = 0;
        float scale = 0.f;
        QRhiTexture::Format format = QRhiTexture::RGBA8;
    };

    void ensureBase();
    bool ensureOffscreen(const QSize &outSize);
    bool createWavePipelines();
    bool createPostPipelines();
    bool createCompositePipeline();
    void rebuildWaveSrb();
    void rebuildPostSrbs();
    void releaseOffscreen();

    XmbWaveRenderData m_data;
    QRhiRenderPassDescriptor *m_outRpDesc = nullptr;

    QShader m_waveVs, m_waveFs, m_postVs, m_downFs, m_blurFs, m_compFs;

    // Wave layer
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ibuf;
    std::unique_ptr<QRhiBuffer> m_lbuf;
    std::unique_ptr<QRhiTexture> m_tex;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_triPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;

    // Offscreen targets
    Offscreen m_off;
    std::unique_ptr<QRhiTexture> m_sceneTex;
    std::unique_ptr<QRhiRenderBuffer> m_sceneMsaa;
    std::unique_ptr<QRhiTextureRenderTarget> m_sceneRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_sceneRpDesc;
    std::unique_ptr<QRhiTexture> m_bloomA;
    std::unique_ptr<QRhiTexture> m_bloomB;
    std::unique_ptr<QRhiTextureRenderTarget> m_bloomRtA;
    std::unique_ptr<QRhiTextureRenderTarget> m_bloomRtB;
    std::unique_ptr<QRhiRenderPassDescriptor> m_bloomRpDesc;

    // Post-processing
    std::unique_ptr<QRhiBuffer> m_quad;
    bool m_quadUploaded = false;
    std::unique_ptr<QRhiSampler> m_postSampler;
    std::unique_ptr<QRhiBuffer> m_downUbuf;
    std::unique_ptr<QRhiBuffer> m_blurHUbuf;
    std::unique_ptr<QRhiBuffer> m_blurVUbuf;
    std::unique_ptr<QRhiBuffer> m_compUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_downSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_blurHSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_blurVSrb;
    std::unique_ptr<QRhiShaderResourceBindings> m_compSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_downPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_blurPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_compPipeline;

    quint64 m_topologySerial = 0;
    quint64 m_textureSerial = 0;
    quint32 m_indexCount = 0;
    quint32 m_lineCount = 0;
    bool m_failed = false;
};

void XmbWaveRenderer::ensureBase()
{
    if (m_ubuf)
        return;
    m_waveVs = loadShader(QStringLiteral(":/shaders/wave.vert.qsb"));
    m_waveFs = loadShader(QStringLiteral(":/shaders/wave.frag.qsb"));
    m_postVs = loadShader(QStringLiteral(":/shaders/post.vert.qsb"));
    m_downFs = loadShader(QStringLiteral(":/shaders/downsample.frag.qsb"));
    m_blurFs = loadShader(QStringLiteral(":/shaders/blur.frag.qsb"));
    m_compFs = loadShader(QStringLiteral(":/shaders/composite.frag.qsb"));
    if (!m_waveVs.isValid() || !m_waveFs.isValid() || !m_postVs.isValid() || !m_downFs.isValid()
        || !m_blurFs.isValid() || !m_compFs.isValid()) {
        qCWarning(xmbRender) << "[RENDER] failed to load shaders";
        m_failed = true;
    }

    auto dyn = [this](quint32 size) {
        std::unique_ptr<QRhiBuffer> b(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size));
        b->create();
        return b;
    };
    m_ubuf = dyn(sizeof(UniformBlock));
    m_downUbuf = dyn(sizeof(PostUniform));
    m_blurHUbuf = dyn(sizeof(PostUniform));
    m_blurVUbuf = dyn(sizeof(PostUniform));
    m_compUbuf = dyn(sizeof(CompositeUniform));

    m_quad.reset(rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuad)));
    m_quad->create();
    m_quadUploaded = false;

    m_sampler.reset(rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_sampler->create();
    m_postSampler.reset(rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_postSampler->create();
    m_tex.reset(rhi()->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
    m_tex->create();
    m_textureSerial = 0;
    rebuildWaveSrb();
}

void XmbWaveRenderer::rebuildWaveSrb()
{
    m_srb.reset(rhi()->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_ubuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_tex.get(),
                                                  m_sampler.get()),
    });
    m_srb->create();
}

void XmbWaveRenderer::rebuildPostSrbs()
{
    const auto fs = QRhiShaderResourceBinding::FragmentStage;
    auto make = [&](QRhiBuffer *ub, QRhiTexture *t) {
        std::unique_ptr<QRhiShaderResourceBindings> s(rhi()->newShaderResourceBindings());
        s->setBindings({QRhiShaderResourceBinding::uniformBuffer(0, fs, ub),
                        QRhiShaderResourceBinding::sampledTexture(1, fs, t, m_postSampler.get())});
        s->create();
        return s;
    };
    m_downSrb = make(m_downUbuf.get(), m_sceneTex.get());
    m_blurHSrb = make(m_blurHUbuf.get(), m_bloomA.get());
    m_blurVSrb = make(m_blurVUbuf.get(), m_bloomB.get());

    m_compSrb.reset(rhi()->newShaderResourceBindings());
    m_compSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, fs, m_compUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1, fs, m_sceneTex.get(), m_postSampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, fs, m_bloomA.get(), m_postSampler.get()),
    });
    m_compSrb->create();
}

void XmbWaveRenderer::releaseOffscreen()
{
    m_triPipeline.reset();
    m_linePipeline.reset();
    m_downPipeline.reset();
    m_blurPipeline.reset();
    m_compPipeline.reset();
    m_downSrb.reset();
    m_blurHSrb.reset();
    m_blurVSrb.reset();
    m_compSrb.reset();
    m_sceneRt.reset();
    m_sceneRpDesc.reset();
    m_sceneMsaa.reset();
    m_sceneTex.reset();
    m_bloomRtA.reset();
    m_bloomRtB.reset();
    m_bloomRpDesc.reset();
    m_bloomA.reset();
    m_bloomB.reset();
    m_off = Offscreen();
}

bool XmbWaveRenderer::ensureOffscreen(const QSize &outSize)
{
    const int maxTex = rhi()->resourceLimit(QRhi::TextureSizeMax);
    float scale = qBound(1.f, m_data.renderScale, 2.f);
    const int longest = qMax(outSize.width(), outSize.height());
    if (longest * scale > maxTex)
        scale = qMax(1.f, float(maxTex) / float(longest));

    int samples = 1;
    const QList<int> supported = rhi()->supportedSampleCounts();
    for (int s : supported) {
        if (s <= m_data.msaaSamples && s > samples)
            samples = s;
    }

    if (m_sceneRt && m_off.outSize == outSize && m_off.samples == samples && qFuzzyCompare(m_off.scale, scale))
        return true;

    releaseOffscreen();

    const QSize sceneSize(qMax(1, int(std::lround(outSize.width() * scale))),
                          qMax(1, int(std::lround(outSize.height() * scale))));
    const QSize bloomSize(qMax(1, outSize.width() / 4), qMax(1, outSize.height() / 4));

    // Prefer a half-float scene: additive layers accumulate without clipping or 8-bit steps.
    QList<QRhiTexture::Format> formats;
    if (rhi()->isTextureFormatSupported(QRhiTexture::RGBA16F))
        formats.append(QRhiTexture::RGBA16F);
    formats.append(QRhiTexture::RGBA8);

    for (QRhiTexture::Format fmt : formats) {
        for (int trySamples : {samples, 1}) {
            m_sceneTex.reset(rhi()->newTexture(fmt, sceneSize, 1, QRhiTexture::RenderTarget));
            if (!m_sceneTex->create())
                continue;
            QRhiColorAttachment att;
            if (trySamples > 1) {
                m_sceneMsaa.reset(rhi()->newRenderBuffer(QRhiRenderBuffer::Color, sceneSize, trySamples, {}, fmt));
                if (!m_sceneMsaa->create()) {
                    m_sceneMsaa.reset();
                    continue;
                }
                att.setRenderBuffer(m_sceneMsaa.get());
                att.setResolveTexture(m_sceneTex.get());
            } else {
                m_sceneMsaa.reset();
                att.setTexture(m_sceneTex.get());
            }
            m_sceneRt.reset(rhi()->newTextureRenderTarget(QRhiTextureRenderTargetDescription(att)));
            m_sceneRpDesc.reset(m_sceneRt->newCompatibleRenderPassDescriptor());
            m_sceneRt->setRenderPassDescriptor(m_sceneRpDesc.get());
            if (!m_sceneRt->create()) {
                m_sceneRt.reset();
                m_sceneRpDesc.reset();
                continue;
            }

            m_bloomA.reset(rhi()->newTexture(fmt, bloomSize, 1, QRhiTexture::RenderTarget));
            m_bloomB.reset(rhi()->newTexture(fmt, bloomSize, 1, QRhiTexture::RenderTarget));
            if (!m_bloomA->create() || !m_bloomB->create())
                continue;
            m_bloomRtA.reset(rhi()->newTextureRenderTarget(QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_bloomA.get()))));
            m_bloomRtB.reset(rhi()->newTextureRenderTarget(QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_bloomB.get()))));
            m_bloomRpDesc.reset(m_bloomRtA->newCompatibleRenderPassDescriptor());
            m_bloomRtA->setRenderPassDescriptor(m_bloomRpDesc.get());
            m_bloomRtB->setRenderPassDescriptor(m_bloomRpDesc.get());
            if (!m_bloomRtA->create() || !m_bloomRtB->create())
                continue;

            m_off.outSize = outSize;
            m_off.sceneSize = sceneSize;
            m_off.bloomSize = bloomSize;
            m_off.samples = trySamples;
            m_off.scale = scale;
            m_off.format = fmt;
            // Remember the *requested* sample count so a fallback does not rebuild every frame.
            m_off.samples = samples;
            rebuildPostSrbs();
            qCInfo(xmbRender) << "[RENDER] offscreen" << sceneSize << "x" << trySamples << "samples"
                              << (fmt == QRhiTexture::RGBA16F ? "RGBA16F" : "RGBA8") << "bloom" << bloomSize;
            return createWavePipelines() && createPostPipelines();
        }
    }
    qCWarning(xmbRender) << "[RENDER] could not create offscreen targets";
    releaseOffscreen();
    return false;
}

bool XmbWaveRenderer::createWavePipelines()
{
    QRhiVertexInputLayout layout;
    layout.setBindings({{6 * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float3, 0},
        {0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)},
    });

    // Additive blending (GMO blend func: add, SRC_ALPHA, ONE). The material alpha is folded into
    // the colour; destination alpha stays 0 so the scene texture is a purely additive layer.
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::One;
    blend.opColor = QRhiGraphicsPipeline::Add;
    blend.srcAlpha = QRhiGraphicsPipeline::Zero;
    blend.dstAlpha = QRhiGraphicsPipeline::One;
    blend.opAlpha = QRhiGraphicsPipeline::Add;

    const int samples = m_sceneMsaa ? m_sceneMsaa->sampleCount() : 1;
    auto make = [&](QRhiGraphicsPipeline::Topology topo) {
        std::unique_ptr<QRhiGraphicsPipeline> p(rhi()->newGraphicsPipeline());
        p->setShaderStages({{QRhiShaderStage::Vertex, m_waveVs}, {QRhiShaderStage::Fragment, m_waveFs}});
        p->setVertexInputLayout(layout);
        p->setShaderResourceBindings(m_srb.get());
        p->setRenderPassDescriptor(m_sceneRpDesc.get());
        p->setSampleCount(samples);
        p->setTopology(topo);
        p->setCullMode(QRhiGraphicsPipeline::None);
        p->setDepthTest(false);
        p->setDepthWrite(false);
        p->setTargetBlends({blend});
        return p->create() ? std::move(p) : nullptr;
    };
    m_triPipeline = make(QRhiGraphicsPipeline::Triangles);
    m_linePipeline = make(QRhiGraphicsPipeline::Lines);
    return m_triPipeline != nullptr;
}

bool XmbWaveRenderer::createPostPipelines()
{
    QRhiVertexInputLayout layout;
    layout.setBindings({{2 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});

    auto make = [&](const QShader &fs, QRhiShaderResourceBindings *srb, QRhiRenderPassDescriptor *rp) {
        std::unique_ptr<QRhiGraphicsPipeline> p(rhi()->newGraphicsPipeline());
        p->setShaderStages({{QRhiShaderStage::Vertex, m_postVs}, {QRhiShaderStage::Fragment, fs}});
        p->setVertexInputLayout(layout);
        p->setShaderResourceBindings(srb);
        p->setRenderPassDescriptor(rp);
        p->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        p->setCullMode(QRhiGraphicsPipeline::None);
        return p->create() ? std::move(p) : nullptr;
    };
    m_downPipeline = make(m_downFs, m_downSrb.get(), m_bloomRpDesc.get());
    m_blurPipeline = make(m_blurFs, m_blurHSrb.get(), m_bloomRpDesc.get());
    return m_downPipeline && m_blurPipeline;
}

bool XmbWaveRenderer::createCompositePipeline()
{
    QRhiVertexInputLayout layout;
    layout.setBindings({{2 * sizeof(float)}});
    layout.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});

    std::unique_ptr<QRhiGraphicsPipeline> p(rhi()->newGraphicsPipeline());
    p->setShaderStages({{QRhiShaderStage::Vertex, m_postVs}, {QRhiShaderStage::Fragment, m_compFs}});
    p->setVertexInputLayout(layout);
    p->setShaderResourceBindings(m_compSrb.get());
    p->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    p->setSampleCount(renderTarget()->sampleCount());
    p->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    p->setCullMode(QRhiGraphicsPipeline::None);
    if (!p->create())
        return false;
    m_compPipeline = std::move(p);
    m_outRpDesc = renderTarget()->renderPassDescriptor();
    return true;
}

void XmbWaveRenderer::initialize(QRhiCommandBuffer *)
{
    if (m_failed)
        return;
    ensureBase();
    // The item's own render target changed (resize / format): drop the composite pipeline.
    if (m_outRpDesc != renderTarget()->renderPassDescriptor())
        m_compPipeline.reset();
}

void XmbWaveRenderer::synchronize(QQuickRhiItem *item)
{
    static_cast<XmbWaveRhiItem *>(item)->syncToRenderer(m_data);
}

void XmbWaveRenderer::render(QRhiCommandBuffer *cb)
{
    const QColor clear(0, 0, 0, 0);
    QRhiResourceUpdateBatch *batch = rhi()->nextResourceUpdateBatch();
    const QSize px = renderTarget()->pixelSize();

    auto emptyPass = [&]() {
        cb->beginPass(renderTarget(), clear, {1.0f, 0}, batch);
        cb->endPass();
    };

    const bool procedural = m_data.ps2Wave || m_data.svecWave;
    const bool sceneWanted = !procedural && m_data.loaded && !m_data.vertices.empty() && m_data.indices
        && !m_data.indices->empty();
    const bool anything = sceneWanted || procedural || m_data.backgroundMode > 0;
    if (m_failed || !anything || px.isEmpty()) {
        emptyPass();
        return;
    }

    ensureBase();
    if (!m_quadUploaded) {
        batch->uploadStaticBuffer(m_quad.get(), kQuad);
        m_quadUploaded = true;
    }

    // Reflection texture. A size change needs a new texture, hence new bindings and pipelines.
    bool waveBindingsChanged = false;
    if (m_data.textureSerial != m_textureSerial && m_data.texture && m_data.texW > 0 && m_data.texH > 0) {
        const QSize sz(m_data.texW, m_data.texH);
        if (m_tex->pixelSize() != sz) {
            m_tex.reset(rhi()->newTexture(QRhiTexture::RGBA8, sz));
            m_tex->create();
            rebuildWaveSrb();
            waveBindingsChanged = true;
        }
        QImage img(reinterpret_cast<const uchar *>(m_data.texture->data()), sz.width(), sz.height(),
                   sz.width() * 4, QImage::Format_RGBA8888);
        batch->uploadTexture(m_tex.get(), img.copy());
        m_textureSerial = m_data.textureSerial;
    }

    if (!ensureOffscreen(px)) {
        m_failed = true;
        emptyPass();
        return;
    }
    if (waveBindingsChanged)
        createWavePipelines();
    if (!m_compPipeline && !createCompositePipeline()) {
        qCWarning(xmbRender) << "[RENDER] composite pipeline failed";
        m_failed = true;
        emptyPass();
        return;
    }

    const bool drawScene = sceneWanted && m_triPipeline;
    const bool drawBloom = drawScene && m_data.bloom > 0.001f && m_downPipeline && m_blurPipeline;

    if (drawScene) {
        // Topology (indices)
        if (m_data.topologySerial != m_topologySerial) {
            const auto &idx = *m_data.indices;
            const quint32 isz = quint32(idx.size() * sizeof(uint32_t));
            m_ibuf.reset(rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, isz));
            m_ibuf->create();
            batch->uploadStaticBuffer(m_ibuf.get(), idx.data());
            m_indexCount = quint32(idx.size());

            m_lbuf.reset();
            m_lineCount = 0;
            if (m_data.lineIndices && !m_data.lineIndices->empty()) {
                const auto &li = *m_data.lineIndices;
                const quint32 lsz = quint32(li.size() * sizeof(uint32_t));
                m_lbuf.reset(rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, lsz));
                m_lbuf->create();
                batch->uploadStaticBuffer(m_lbuf.get(), li.data());
                m_lineCount = quint32(li.size());
            }
            m_topologySerial = m_data.topologySerial;
        }

        // Vertices (every frame)
        const quint32 vsz = quint32(m_data.vertices.size() * sizeof(float));
        if (!m_vbuf || m_vbuf->size() != vsz) {
            m_vbuf.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, vsz));
            m_vbuf->create();
        }
        batch->updateDynamicBuffer(m_vbuf.get(), 0, vsz, m_data.vertices.data());

        // Camera: z = 0 plane shows the PSP screen (480 units wide) across the full item width.
        const float aspect = float(px.width()) / float(qMax(1, px.height()));
        const float dist = (kPspHeight * 0.5f) / std::tan(qDegreesToRadians(m_data.fovY) * 0.5f);
        const float fovY = 2.f * std::atan((kPspWidth * 0.5f) / (aspect * dist));

        QMatrix4x4 mvp = rhi()->clipSpaceCorrMatrix();
        mvp.translate(0.f, 1.f - 2.f * m_data.verticalCenter, 0.f);
        mvp.perspective(qRadiansToDegrees(fovY), aspect, dist * 0.1f, dist * 4.f);
        mvp.lookAt(QVector3D(0.f, 0.f, dist), QVector3D(0.f, 0.f, 0.f), QVector3D(0.f, 1.f, 0.f));

        UniformBlock u{};
        std::memcpy(u.mvp, mvp.constData(), sizeof(u.mvp));
        std::copy_n(m_data.color, 4, u.color);
        u.params[0] = float(int(m_data.debugMode));
        u.params[1] = m_data.bicubic ? 1.f : 0.f;
        u.params[2] = 1.f / float(qMax(1, m_tex->pixelSize().width()));
        u.params[3] = 1.f / float(qMax(1, m_tex->pixelSize().height()));
        batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(u), &u);
    }

    if (drawBloom) {
        const QSize ss = m_off.sceneSize;
        const QSize bs = m_off.bloomSize;
        PostUniform d{};
        d.texel[0] = 1.f / float(ss.width());
        d.texel[1] = 1.f / float(ss.height());
        d.texel[2] = float(ss.width()) / float(bs.width());
        d.texel[3] = float(ss.height()) / float(bs.height());
        batch->updateDynamicBuffer(m_downUbuf.get(), 0, sizeof(d), &d);
        // Blur radius follows the output height so the glow looks the same at 1080p and 4K.
        const float spread = qBound(1.f, float(px.height()) / 1080.f * 1.25f, 3.f);
        PostUniform h{};
        h.texel[0] = 1.f / float(bs.width());
        h.texel[1] = 1.f / float(bs.height());
        h.texel[2] = spread;
        batch->updateDynamicBuffer(m_blurHUbuf.get(), 0, sizeof(h), &h);
        PostUniform v = h;
        v.texel[2] = 0.f;
        v.texel[3] = spread;
        batch->updateDynamicBuffer(m_blurVUbuf.get(), 0, sizeof(v), &v);
    }

    CompositeUniform c{};
    std::copy_n(m_data.bgTop, 3, c.bgTop);
    std::copy_n(m_data.bgBottom, 3, c.bgBottom);
    c.bgParams[0] = float(m_data.backgroundMode);
    c.bgParams[1] = qDegreesToRadians(m_data.bgAngle);
    c.bgParams[2] = m_data.brightness;
    c.bgParams[3] = rhi()->isYUpInNDC() ? 1.f : 0.f;
    std::copy_n(m_data.ps2Color, 3, c.ps2Color);
    c.ps2Color[3] = m_data.ps2Wave ? 1.f : 0.f;
    std::copy_n(m_data.svec, 8, c.svec);
    c.fx[0] = (drawBloom || !drawScene) ? m_data.bloom : 0.f; // PS2 wave glow uses it too
    c.fx[1] = m_data.vignette;
    c.fx[2] = m_data.dither;
    c.fx[3] = m_data.time;
    c.view[0] = float(px.width());
    c.view[1] = float(px.height());
    c.view[2] = m_data.verticalCenter;
    c.view[3] = m_data.ps2Opacity;
    c.scene[0] = 1.f / float(px.width());
    c.scene[1] = 1.f / float(px.height());
    c.scene[2] = drawScene ? 1.f : 0.f;
    c.scene[3] = m_off.scale;
    batch->updateDynamicBuffer(m_compUbuf.get(), 0, sizeof(c), &c);

    const QRhiCommandBuffer::VertexInput quad(m_quad.get(), 0);
    if (drawScene) {
        const QSize ss = m_off.sceneSize;
        cb->beginPass(m_sceneRt.get(), clear, {1.0f, 0}, batch);
        batch = nullptr;
        cb->setViewport({0, 0, float(ss.width()), float(ss.height())});
        const QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        if (m_data.debugMode == XmbDebugRenderMode::Wireframe && m_linePipeline && m_lbuf) {
            cb->setGraphicsPipeline(m_linePipeline.get());
            cb->setShaderResources(m_srb.get());
            cb->setVertexInput(0, 1, &vin, m_lbuf.get(), 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(m_lineCount);
        } else {
            cb->setGraphicsPipeline(m_triPipeline.get());
            cb->setShaderResources(m_srb.get());
            cb->setVertexInput(0, 1, &vin, m_ibuf.get(), 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(m_indexCount);
        }
        cb->endPass();
    }

    if (drawBloom) {
        const QSize bs = m_off.bloomSize;
        auto post = [&](QRhiTextureRenderTarget *rt, QRhiGraphicsPipeline *pipe, QRhiShaderResourceBindings *srb) {
            cb->beginPass(rt, clear, {1.0f, 0}, batch);
            batch = nullptr;
            cb->setGraphicsPipeline(pipe);
            cb->setViewport({0, 0, float(bs.width()), float(bs.height())});
            cb->setShaderResources(srb);
            cb->setVertexInput(0, 1, &quad);
            cb->draw(4);
            cb->endPass();
        };
        post(m_bloomRtA.get(), m_downPipeline.get(), m_downSrb.get());
        post(m_bloomRtB.get(), m_blurPipeline.get(), m_blurHSrb.get());
        post(m_bloomRtA.get(), m_blurPipeline.get(), m_blurVSrb.get());
    }

    cb->beginPass(renderTarget(), clear, {1.0f, 0}, batch);
    cb->setGraphicsPipeline(m_compPipeline.get());
    cb->setViewport({0, 0, float(px.width()), float(px.height())});
    cb->setShaderResources(m_compSrb.get());
    cb->setVertexInput(0, 1, &quad);
    cb->draw(4);
    cb->endPass();
}

// --- GUI thread ---------------------------------------------------------------------------------

XmbWaveRhiItem::XmbWaveRhiItem(QQuickItem *parent)
    : QQuickRhiItem(parent)
{
    setColorBufferFormat(QQuickRhiItem::TextureFormat::RGBA8);
    setAlphaBlending(true);
    setFlag(QQuickItem::ItemHasContents, true);
    m_debugOverlay = debugEnabledFromEnv();

    m_clock.start();
    m_lastTickNs = m_clock.nsecsElapsed();
    connect(&m_timer, &QTimer::timeout, this, &XmbWaveRhiItem::tick);
    m_timer.setTimerType(Qt::PreciseTimer);
    updateTimer();

    m_onBattery = systemOnBattery();
    m_batteryTimer.setInterval(10000);
    connect(&m_batteryTimer, &QTimer::timeout, this, [this]() {
        const bool b = systemOnBattery();
        if (b != m_onBattery) {
            m_onBattery = b;
            updateEffectiveTessLevel();
            updateTimer();
        }
    });
    m_batteryTimer.start();
}

XmbWaveRhiItem::~XmbWaveRhiItem()
{
    ++m_loadGeneration;
}

QQuickRhiItemRenderer *XmbWaveRhiItem::createRenderer()
{
    return new XmbWaveRenderer();
}

void XmbWaveRhiItem::syncToRenderer(XmbWaveRenderData &out) const
{
    out = m_cache;
    const float o = float(m_waveOpacity);
    if (m_wave) {
        // GE texture function MODULATE with the material diffuse, then blend SRC_ALPHA/ONE.
        const float a = m_wave->alpha * m_wave->tint[3] * o;
        const float tint[3] = {float(m_waveTint.redF()), float(m_waveTint.greenF()), float(m_waveTint.blueF())};
        for (int i = 0; i < 3; ++i)
            out.color[i] = m_wave->diffuse[i] * m_wave->tint[i] * a * tint[i];
        out.color[3] = 1.f;
    }
    out.verticalCenter = float(m_waveCenterY);
    out.fovY = kPspFovY;
    out.debugMode = m_debugMode;
    out.loaded = m_loaded;

    out.renderScale = float(m_renderScale);
    out.msaaSamples = m_msaaSamples;
    out.bloom = float(m_bloom);
    out.bicubic = m_bicubic;
    out.dither = m_dither ? 1.f : 0.f;
    out.vignette = float(m_vignette);

    out.backgroundMode = m_backgroundMode;
    const auto rgb = [](const QColor &c, float *dst) {
        dst[0] = float(c.redF());
        dst[1] = float(c.greenF());
        dst[2] = float(c.blueF());
    };
    rgb(m_bgTop, out.bgTop);
    rgb(m_bgBottom, out.bgBottom);
    out.bgAngle = float(m_bgAngle);
    out.brightness = float(m_brightness);

    out.ps2Wave = m_waveStyle == 1;
    out.svecWave = m_waveStyle == 2;
    rgb(m_ps2Color, out.ps2Color);
    out.ps2Opacity = qMin(1.f, o);

    // Svec Studio wave (simple hero waves): wave colour, enabled flag and whether
    // the colour is a user pick (custom palette) or derived from the theme.
    {
        float *s = out.svec;
        std::fill_n(s, 8, 0.f);
        rgb(m_svecWaveColor, s + 0);
        s[3] = out.svecWave ? 1.f : 0.f;
        s[4] = m_svecWaveCustom ? 1.f : 0.f;
    }
    out.time = float(std::fmod(m_time, 36000.0));
}

bool XmbWaveRhiItem::animating() const
{
    return !m_paused && !(m_pauseOnBattery && m_onBattery);
}

void XmbWaveRhiItem::updateTimer()
{
    int fps = m_maxFps;
    if (m_batterySaver || (m_pauseOnBattery && m_onBattery))
        fps = qMin(fps, 15);
    m_timer.setInterval(qMax(1, 1000 / qMax(1, fps)));
    if ((m_loaded || proceduralActive()) && animating()) {
        if (!m_timer.isActive()) {
            m_lastTickNs = m_clock.nsecsElapsed();
            m_timer.start();
        }
    } else {
        m_timer.stop();
    }
}

void XmbWaveRhiItem::tick()
{
    const qint64 now = m_clock.nsecsElapsed();
    float dtMs = float(double(now - m_lastTickNs) / 1e6);
    m_lastTickNs = now;
    if (dtMs < 0.f || dtMs > 250.f)
        dtMs = 1000.f / float(m_maxFps);

    m_time += double(dtMs) * 0.001 * m_speed;
    if (!m_wave) {
        if (proceduralActive())
            update();
        return;
    }
    const float loop = m_wave->loopFrames();
    m_frame += dtMs * 0.001f * m_wave->frameRate * float(m_speed);
    if (loop > 0.f)
        m_frame = std::fmod(m_frame, loop);

    if (proceduralActive()) {
        update();
        return;
    }

    QElapsedTimer cost;
    cost.start();
    evaluateMesh();
    m_frameCostMs.push_back(float(cost.nsecsElapsed()) / 1e6f);
    if (m_frameCostMs.size() > 60)
        m_frameCostMs.erase(m_frameCostMs.begin());

    ++m_fpsFrames;
    if (now - m_fpsWindowStart > 1000000000LL) {
        m_fps = float(m_fpsFrames) * 1e9f / float(now - m_fpsWindowStart);
        m_fpsFrames = 0;
        m_fpsWindowStart = now;
        if (m_adaptiveQuality)
            updateEffectiveTessLevel();
        updateDiagnostics();
    }
    emit phaseChanged();
    update();
}

void XmbWaveRhiItem::evaluateMesh()
{
    if (!m_wave || !m_tess.valid())
        return;
    m_tess.evaluate(*m_wave, m_wave->frameStart + m_frame, m_cache.vertices);
}

void XmbWaveRhiItem::rebuildTopology()
{
    if (!m_wave)
        return;
    int us = 0, vs = 0;
    segmentsForLevel(m_effectiveTessLevel, us, vs);
    m_tess.setup(*m_wave, us, vs);
    m_cache.indices = std::make_shared<std::vector<uint32_t>>(m_tess.indices());
    m_cache.lineIndices = std::make_shared<std::vector<uint32_t>>(m_tess.lineIndices());
    m_cache.topologySerial = ++m_topologySerial;
    evaluateMesh();
}

void XmbWaveRhiItem::updateEffectiveTessLevel()
{
    int level = m_tessLevel;
    if (m_batterySaver || (m_pauseOnBattery && m_onBattery))
        level = qMax(1, level - 1);
    if (m_adaptiveQuality && m_frameCostMs.size() >= 30) {
        float avg = 0.f;
        for (float c : m_frameCostMs)
            avg += c;
        avg /= float(m_frameCostMs.size());
        if (avg > 6.f)
            level = qMax(1, qMin(level, m_effectiveTessLevel - 1));
    }
    level = qBound(1, level, 4);
    if (level == m_effectiveTessLevel)
        return;
    m_effectiveTessLevel = level;
    m_frameCostMs.clear();
    emit effectiveTessLevelChanged();
    rebuildTopology();
    update();
}

void XmbWaveRhiItem::setRcoPath(const QString &path)
{
    QString p = path.trimmed();
    if (p.startsWith(QStringLiteral("file://")))
        p = QUrl(p).toLocalFile();
    if (p == m_rcoPath)
        return;
    m_rcoPath = p;
    emit rcoPathChanged();
    QMetaObject::invokeMethod(this, &XmbWaveRhiItem::reload, Qt::QueuedConnection);
}

void XmbWaveRhiItem::setCollectionPath(const QString &path)
{
    QString p = path.trimmed();
    if (p.startsWith(QStringLiteral("file://")))
        p = QUrl(p).toLocalFile();
    if (p == m_collectionPath)
        return;
    m_collectionPath = p;
    emit collectionPathChanged();
    if (!m_collectionPath.isEmpty() && !m_waveName.isEmpty())
        setRcoPath(QDir(m_collectionPath).filePath(m_waveName + QStringLiteral("/system_plugin_bg.rco")));
}

void XmbWaveRhiItem::setWaveName(const QString &name)
{
    if (name == m_waveName)
        return;
    m_waveName = name;
    emit waveNameChanged();
    if (!m_collectionPath.isEmpty() && !m_waveName.isEmpty())
        setRcoPath(QDir(m_collectionPath).filePath(m_waveName + QStringLiteral("/system_plugin_bg.rco")));
}

bool XmbWaveRhiItem::reload()
{
    if (m_rcoPath.isEmpty()) {
        ++m_loadGeneration;
        m_wave.reset();
        m_loaded = false;
        m_cache.loaded = false;
        setErrorString(QString());
        setLoadStatus(QStringLiteral("loading"));
        emit loadedChanged();
        updateTimer();
        update();
        return false;
    }
    if (!QFileInfo::exists(m_rcoPath)) {
        ++m_loadGeneration;
        qCWarning(xmbRco) << "[RCO] file not found:" << m_rcoPath;
        m_wave.reset();
        m_loaded = false;
        m_cache.loaded = false;
        if (m_loading) {
            m_loading = false;
            emit loadingChanged();
        }
        setErrorString(QStringLiteral("RCO file not found"));
        setLoadStatus(QStringLiteral("error"));
        emit loadedChanged();
        updateTimer();
        update();
        return false;
    }
    startAsyncLoad();
    return true;
}

void XmbWaveRhiItem::startAsyncLoad()
{
    const int gen = ++m_loadGeneration;
    const std::string path = m_rcoPath.toStdString();
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }
    setLoadStatus(QStringLiteral("loading"));

    struct Outcome {
        std::shared_ptr<xmb::PspWave> wave;
        xmb::PspWaveLoadResult result;
    };
    auto *watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcher<Outcome>::finished, this, [this, watcher, gen]() {
        const Outcome out = watcher->result();
        watcher->deleteLater();
        if (gen != m_loadGeneration.load())
            return;
        m_loading = false;
        emit loadingChanged();

        if (!out.result.ok) {
            qCWarning(xmbRco) << "[RCO] load failed:" << m_rcoPath << QString::fromStdString(out.result.error);
            m_wave.reset();
            m_loaded = false;
            m_cache.loaded = false;
            setErrorString(QString::fromStdString(out.result.error));
            setLoadStatus(QStringLiteral("error"));
            emit loadError(m_errorString);
            emit loadedChanged();
            updateTimer();
            update();
            return;
        }

        m_wave = out.wave;
        qCInfo(xmbRco) << "[RCO] loaded" << m_rcoPath << QString::fromStdString(m_wave->log);
        m_frame = 0.f;
        m_frameCostMs.clear();
        m_effectiveTessLevel = m_tessLevel;
        if (m_batterySaver || (m_pauseOnBattery && m_onBattery))
            m_effectiveTessLevel = qMax(1, m_tessLevel - 1);
        emit effectiveTessLevelChanged();

        m_cache.texture = std::make_shared<std::vector<uint8_t>>(m_wave->rgba);
        m_cache.texW = m_wave->texW;
        m_cache.texH = m_wave->texH;
        if (m_wave->rgba.empty()) {
            // No texture: plain white sphere map so the geometry still shows.
            m_cache.texture = std::make_shared<std::vector<uint8_t>>(4, uint8_t(255));
            m_cache.texW = m_cache.texH = 1;
        }
        m_cache.textureSerial = ++m_textureSerial;
        rebuildTopology();

        m_loaded = true;
        m_cache.loaded = true;
        setErrorString(QString());
        setLoadStatus(QStringLiteral("ready"));
        updateDiagnostics();
        emit loadedChanged();
        updateTimer();
        update();
    });

    watcher->setFuture(QtConcurrent::run([path]() {
        Outcome out;
        auto wave = std::make_shared<xmb::PspWave>();
        out.result = xmb::loadPspWaveRco(path, *wave);
        if (out.result.ok)
            out.wave = std::move(wave);
        return out;
    }));
}

void XmbWaveRhiItem::setLoadStatus(const QString &s)
{
    if (s == m_loadStatus)
        return;
    m_loadStatus = s;
    emit loadStatusChanged();
}

void XmbWaveRhiItem::setErrorString(const QString &e)
{
    if (e == m_errorString)
        return;
    m_errorString = e;
    emit errorChanged();
}

void XmbWaveRhiItem::setTessLevel(int level)
{
    level = qBound(1, level, 4);
    if (level == m_tessLevel)
        return;
    m_tessLevel = level;
    emit tessLevelChanged();
    m_frameCostMs.clear();
    m_effectiveTessLevel = 0;
    updateEffectiveTessLevel();
}

void XmbWaveRhiItem::setMaxFps(int fps)
{
    fps = qBound(5, fps, 144);
    if (fps == m_maxFps)
        return;
    m_maxFps = fps;
    emit maxFpsChanged();
    updateTimer();
}

void XmbWaveRhiItem::setPaused(bool p)
{
    if (p == m_paused)
        return;
    m_paused = p;
    emit pausedChanged();
    updateTimer();
}

void XmbWaveRhiItem::setPauseOnBattery(bool p)
{
    if (p == m_pauseOnBattery)
        return;
    m_pauseOnBattery = p;
    emit pauseOnBatteryChanged();
    updateEffectiveTessLevel();
    updateTimer();
}

void XmbWaveRhiItem::setBatterySaver(bool v)
{
    if (v == m_batterySaver)
        return;
    m_batterySaver = v;
    emit batterySaverChanged();
    updateEffectiveTessLevel();
    updateTimer();
}

void XmbWaveRhiItem::setAdaptiveQuality(bool v)
{
    if (v == m_adaptiveQuality)
        return;
    m_adaptiveQuality = v;
    emit adaptiveQualityChanged();
    updateEffectiveTessLevel();
}

void XmbWaveRhiItem::setWaveOpacity(qreal o)
{
    o = qBound(0.05, o, 2.0);
    if (qFuzzyCompare(o, m_waveOpacity))
        return;
    m_waveOpacity = o;
    emit waveOpacityChanged();
    update();
}

void XmbWaveRhiItem::setWaveCenterY(qreal c)
{
    c = qBound(0.0, c, 1.0);
    if (qFuzzyCompare(c + 1.0, m_waveCenterY + 1.0))
        return;
    m_waveCenterY = c;
    emit waveCenterYChanged();
    update();
}

void XmbWaveRhiItem::setSpeed(qreal s)
{
    s = qBound(0.05, s, 8.0);
    if (qFuzzyCompare(s, m_speed))
        return;
    m_speed = s;
    emit speedChanged();
}

void XmbWaveRhiItem::setDebugOverlay(bool v)
{
    if (v == m_debugOverlay)
        return;
    m_debugOverlay = v;
    emit debugOverlayChanged();
}

bool XmbWaveRhiItem::debugOverlayEnabled() const
{
    return m_debugOverlay || debugEnabledFromEnv();
}

void XmbWaveRhiItem::setDebugRenderMode(int mode)
{
    const auto m = static_cast<XmbDebugRenderMode>(qBound(0, mode, 3));
    if (m == m_debugMode)
        return;
    m_debugMode = m;
    emit debugRenderModeChanged();
    update();
}

void XmbWaveRhiItem::setWireframeMode(bool v)
{
    setDebugRenderMode(int(v ? XmbDebugRenderMode::Wireframe : XmbDebugRenderMode::Normal));
}

void XmbWaveRhiItem::setRenderScale(qreal s)
{
    s = qBound(1.0, s, 2.0);
    if (qFuzzyCompare(s, m_renderScale))
        return;
    m_renderScale = s;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setMsaaSamples(int n)
{
    n = n >= 8 ? 8 : (n >= 4 ? 4 : (n >= 2 ? 2 : 1));
    if (n == m_msaaSamples)
        return;
    m_msaaSamples = n;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setBloom(qreal b)
{
    b = qBound(0.0, b, 2.0);
    if (qFuzzyCompare(b + 1.0, m_bloom + 1.0))
        return;
    m_bloom = b;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setBicubicTexture(bool v)
{
    if (v == m_bicubic)
        return;
    m_bicubic = v;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setDither(bool v)
{
    if (v == m_dither)
        return;
    m_dither = v;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setVignette(qreal v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v + 1.0, m_vignette + 1.0))
        return;
    m_vignette = v;
    emit renderQualityChanged();
    update();
}

void XmbWaveRhiItem::setBackgroundMode(int m)
{
    m = qBound(0, m, 2);
    if (m == m_backgroundMode)
        return;
    m_backgroundMode = m;
    emit backgroundChanged();
    update();
}

void XmbWaveRhiItem::setBackgroundTop(const QColor &c)
{
    if (c == m_bgTop)
        return;
    m_bgTop = c;
    emit backgroundChanged();
    update();
}

void XmbWaveRhiItem::setBackgroundBottom(const QColor &c)
{
    if (c == m_bgBottom)
        return;
    m_bgBottom = c;
    emit backgroundChanged();
    update();
}

void XmbWaveRhiItem::setBackgroundAngle(qreal a)
{
    if (qFuzzyCompare(a, m_bgAngle))
        return;
    m_bgAngle = a;
    emit backgroundChanged();
    update();
}

void XmbWaveRhiItem::setBrightness(qreal b)
{
    b = qBound(0.1, b, 1.5);
    if (qFuzzyCompare(b, m_brightness))
        return;
    m_brightness = b;
    emit backgroundChanged();
    update();
}

void XmbWaveRhiItem::setWaveStyle(int s)
{
    s = qBound(0, s, 2);
    if (s == m_waveStyle)
        return;
    m_waveStyle = s;
    emit waveStyleChanged();
    updateTimer();
    update();
}

void XmbWaveRhiItem::setPs2WaveColor(const QColor &c)
{
    if (c == m_ps2Color)
        return;
    m_ps2Color = c;
    emit waveStyleChanged();
    update();
}

void XmbWaveRhiItem::setWaveTint(const QColor &c)
{
    if (c == m_waveTint)
        return;
    m_waveTint = c;
    emit waveStyleChanged();
    update();
}

void XmbWaveRhiItem::setSvecWaveColor(const QColor &c)
{
    if (c == m_svecWaveColor)
        return;
    m_svecWaveColor = c;
    emit waveStyleChanged();
    update();
}

void XmbWaveRhiItem::setSvecWaveCustom(bool v)
{
    if (v == m_svecWaveCustom)
        return;
    m_svecWaveCustom = v;
    emit waveStyleChanged();
    update();
}

QString XmbWaveRhiItem::defaultScreenshotDir()
{
    return QDir::homePath() + QStringLiteral("/.cache/vlnky/screenshots");
}

void XmbWaveRhiItem::captureScreenshot(const QString &label)
{
    const QString dir = defaultScreenshotDir();
    QDir().mkpath(dir);
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString path = dir + QLatin1Char('/') + ts + QLatin1Char('_')
        + (label.isEmpty() ? QStringLiteral("wave") : label) + QStringLiteral(".png");
    const QSharedPointer<QQuickItemGrabResult> grab = grabToImage();
    if (!grab)
        return;
    connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab, path]() {
        if (!grab->image().isNull() && grab->image().save(path))
            emit screenshotCaptured(path);
    });
}

void XmbWaveRhiItem::captureAllDebugScreenshots()
{
    const QString dir = defaultScreenshotDir();
    QDir().mkpath(dir);
    const QString path = dir + QLatin1Char('/')
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")) + QStringLiteral(".png");
    const QSharedPointer<QQuickItemGrabResult> grab = grabToImage();
    if (!grab)
        return;
    connect(grab.data(), &QQuickItemGrabResult::ready, this, [this, grab, path, dir]() {
        if (!grab->image().isNull() && grab->image().save(path))
            emit screenshotCaptured(path);
        emit allScreenshotsCaptured(dir);
    });
}

QStringList XmbWaveRhiItem::scanWaves() const
{
    QStringList names;
    if (m_collectionPath.isEmpty())
        return names;
    const QDir dir(m_collectionPath);
    for (const QString &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (QFileInfo(dir.filePath(entry + QStringLiteral("/system_plugin_bg.rco"))).isFile())
            names.append(entry);
    }
    return names;
}

void XmbWaveRhiItem::updateDiagnostics()
{
    QString s;
    if (m_wave) {
        float cost = 0.f;
        for (float c : m_frameCostMs)
            cost += c;
        if (!m_frameCostMs.empty())
            cost /= float(m_frameCostMs.size());
        s = QStringLiteral("%1\n%2\nframe %3/%4  fps %5  tess %6 (%7x%8)  cpu %9 ms")
                .arg(QFileInfo(QFileInfo(m_rcoPath).path()).fileName(), QString::fromStdString(m_wave->log))
                .arg(double(m_frame), 0, 'f', 0)
                .arg(double(m_wave->loopFrames()), 0, 'f', 0)
                .arg(double(m_fps), 0, 'f', 1)
                .arg(m_effectiveTessLevel)
                .arg(m_tess.uSamples())
                .arg(m_tess.vSamples())
                .arg(double(cost), 0, 'f', 2);
    } else {
        s = m_loadStatus + (m_errorString.isEmpty() ? QString() : QStringLiteral(": ") + m_errorString);
    }
    if (s != m_diagnostics) {
        m_diagnostics = s;
        emit diagnosticsChanged();
    }
}
