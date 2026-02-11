#include "GpuPathTracer.h"

#include <QOpenGLContext>
#include <algorithm>
#include <vector>

GpuPathTracer::GpuPathTracer() {
}

GpuPathTracer::~GpuPathTracer() {
    release();
}

bool GpuPathTracer::initialize() {
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        m_lastError = QStringLiteral("No current OpenGL context");
        return false;
    }

    initializeOpenGLFunctions();

    const QSurfaceFormat fmt = ctx->format();
    const bool has43 = (fmt.majorVersion() > 4) || (fmt.majorVersion() == 4 && fmt.minorVersion() >= 3);
    if (!has43) {
        m_lastError = QStringLiteral("OpenGL 4.3+ is required for compute shaders");
        return false;
    }

    if (!ensureProgram()) {
        return false;
    }

    m_ready = true;
    return true;
}

bool GpuPathTracer::resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (m_width == width && m_height == height && m_accumTex != 0 && m_outputTex != 0) {
        return true;
    }

    m_width = width;
    m_height = height;
    m_frameIndex = 0;
    return ensureTextures();
}

bool GpuPathTracer::renderFrame(int samplesPerFrame, int maxDepth) {
    if (!m_ready || !m_program || m_accumTex == 0 || m_outputTex == 0) {
        return false;
    }

    samplesPerFrame = std::max(1, samplesPerFrame);
    maxDepth = std::clamp(maxDepth, 1, 64);

    m_program->bind();
    m_program->setUniformValue("uWidth", m_width);
    m_program->setUniformValue("uHeight", m_height);
    m_program->setUniformValue("uMaxDepth", maxDepth);

    glBindImageTexture(0, m_outputTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glBindImageTexture(1, m_accumTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

    const GLuint groupsX = static_cast<GLuint>((m_width + 7) / 8);
    const GLuint groupsY = static_cast<GLuint>((m_height + 7) / 8);

    for (int i = 0; i < samplesPerFrame; ++i) {
        m_program->setUniformValue("uFrameIndex", m_frameIndex);
        glDispatchCompute(groupsX, groupsY, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        ++m_frameIndex;
    }

    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    m_program->release();
    return true;
}

void GpuPathTracer::resetAccumulation() {
    m_frameIndex = 0;
    if (m_accumTex == 0) {
        return;
    }

    std::vector<float> zeroData(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4, 0.0f);
    glBindTexture(GL_TEXTURE_2D, m_accumTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, zeroData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool GpuPathTracer::isReady() const {
    return m_ready;
}

GLuint GpuPathTracer::outputTextureId() const {
    return m_outputTex;
}

int GpuPathTracer::width() const {
    return m_width;
}

int GpuPathTracer::height() const {
    return m_height;
}

int GpuPathTracer::frameIndex() const {
    return m_frameIndex;
}

QString GpuPathTracer::lastError() const {
    return m_lastError;
}

void GpuPathTracer::release() {
    if (m_accumTex != 0) {
        glDeleteTextures(1, &m_accumTex);
        m_accumTex = 0;
    }
    if (m_outputTex != 0) {
        glDeleteTextures(1, &m_outputTex);
        m_outputTex = 0;
    }
    m_program.reset();
    m_ready = false;
}

bool GpuPathTracer::ensureProgram() {
    if (m_program) {
        return true;
    }

    static const char *computeShader = R"(
#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, rgba8) uniform writeonly image2D outImage;
layout(binding = 1, rgba32f) uniform image2D accumImage;

uniform int uWidth;
uniform int uHeight;
uniform int uFrameIndex;
uniform int uMaxDepth;

uint hash(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float rand01(inout uint state) {
    state = hash(state);
    return float(state) / 4294967295.0;
}

vec3 randomInUnitSphere(inout uint state) {
    while (true) {
        vec3 p = vec3(rand01(state), rand01(state), rand01(state)) * 2.0 - 1.0;
        if (dot(p, p) < 1.0) {
            return p;
        }
    }
}

bool hitSphere(vec3 center, float radius, vec3 ro, vec3 rd, out float t, out vec3 normal, out vec3 albedo) {
    vec3 oc = ro - center;
    float a = dot(rd, rd);
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float d = b * b - a * c;
    if (d < 0.0) {
        return false;
    }
    float s = sqrt(d);
    float t0 = (-b - s) / a;
    float t1 = (-b + s) / a;
    t = t0 > 0.001 ? t0 : t1;
    if (t <= 0.001) {
        return false;
    }
    vec3 p = ro + t * rd;
    normal = normalize(p - center);
    if (radius > 50.0) {
        albedo = vec3(0.8, 0.8, 0.0);
    } else if (center.x < -0.5) {
        albedo = vec3(0.8, 0.3, 0.3);
    } else if (center.x > 0.5) {
        albedo = vec3(0.3, 0.8, 0.3);
    } else {
        albedo = vec3(0.75);
    }
    return true;
}

vec3 traceRay(vec3 ro, vec3 rd, inout uint state, int maxDepth) {
    vec3 throughput = vec3(1.0);
    vec3 radiance = vec3(0.0);

    for (int depth = 0; depth < maxDepth; ++depth) {
        float bestT = 1e20;
        vec3 bestN = vec3(0.0);
        vec3 bestAlbedo = vec3(0.0);
        bool hit = false;

        float t;
        vec3 n;
        vec3 albedo;
        if (hitSphere(vec3(0.0, -100.5, -1.0), 100.0, ro, rd, t, n, albedo) && t < bestT) {
            bestT = t;
            bestN = n;
            bestAlbedo = albedo;
            hit = true;
        }
        if (hitSphere(vec3(0.0, 0.0, -1.0), 0.5, ro, rd, t, n, albedo) && t < bestT) {
            bestT = t;
            bestN = n;
            bestAlbedo = albedo;
            hit = true;
        }
        if (hitSphere(vec3(-1.0, 0.0, -1.4), 0.5, ro, rd, t, n, albedo) && t < bestT) {
            bestT = t;
            bestN = n;
            bestAlbedo = albedo;
            hit = true;
        }
        if (hitSphere(vec3(1.0, 0.0, -1.2), 0.5, ro, rd, t, n, albedo) && t < bestT) {
            bestT = t;
            bestN = n;
            bestAlbedo = albedo;
            hit = true;
        }

        if (!hit) {
            vec3 unit = normalize(rd);
            float a = 0.5 * (unit.y + 1.0);
            vec3 sky = mix(vec3(1.0), vec3(0.5, 0.7, 1.0), a);
            radiance += throughput * sky;
            break;
        }

        vec3 hitPos = ro + bestT * rd;
        vec3 scatterDir = normalize(bestN + randomInUnitSphere(state));
        ro = hitPos + bestN * 0.001;
        rd = scatterDir;
        throughput *= bestAlbedo;
    }

    return radiance;
}

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= uWidth || p.y >= uHeight) {
        return;
    }

    uint seed = uint((p.x + p.y * uWidth) * 9781 + (uFrameIndex + 1) * 6271);
    float u = (float(p.x) + rand01(seed)) / float(max(1, uWidth - 1));
    float v = (float(p.y) + rand01(seed)) / float(max(1, uHeight - 1));

    float aspect = float(uWidth) / float(uHeight);
    vec3 origin = vec3(0.0, 0.3, 1.2);
    vec3 lowerLeft = vec3(-aspect, -1.0, -1.0);
    vec3 horizontal = vec3(2.0 * aspect, 0.0, 0.0);
    vec3 vertical = vec3(0.0, 2.0, 0.0);
    vec3 rd = normalize(lowerLeft + u * horizontal + v * vertical - origin);

    vec3 sampleColor = traceRay(origin, rd, seed, uMaxDepth);

    vec4 prev = imageLoad(accumImage, p);
    float frameCount = float(uFrameIndex + 1);
    vec3 accum = (prev.rgb * float(uFrameIndex) + sampleColor) / frameCount;
    imageStore(accumImage, p, vec4(accum, 1.0));

    vec3 mapped = sqrt(clamp(accum, vec3(0.0), vec3(1.0)));
    imageStore(outImage, p, vec4(mapped, 1.0));
}
    )";

    m_program = std::make_unique<QOpenGLShaderProgram>();
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Compute, computeShader)) {
        m_lastError = QStringLiteral("Compute shader compile failed: ") + m_program->log();
        m_program.reset();
        return false;
    }
    if (!m_program->link()) {
        m_lastError = QStringLiteral("Compute shader link failed: ") + m_program->log();
        m_program.reset();
        return false;
    }

    return true;
}

bool GpuPathTracer::ensureTextures() {
    if (m_width <= 0 || m_height <= 0) {
        return false;
    }

    if (m_accumTex != 0) {
        glDeleteTextures(1, &m_accumTex);
        m_accumTex = 0;
    }
    if (m_outputTex != 0) {
        glDeleteTextures(1, &m_outputTex);
        m_outputTex = 0;
    }

    glGenTextures(1, &m_accumTex);
    glBindTexture(GL_TEXTURE_2D, m_accumTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, m_width, m_height);

    std::vector<float> zeroData(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4, 0.0f);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_FLOAT, zeroData.data());

    glGenTextures(1, &m_outputTex);
    glBindTexture(GL_TEXTURE_2D, m_outputTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_width, m_height);

    std::vector<unsigned char> zeroOut(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 4, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, zeroOut.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}
