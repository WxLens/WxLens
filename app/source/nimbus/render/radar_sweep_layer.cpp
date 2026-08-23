#include <nimbus/render/radar_sweep_layer.hpp>
#include <nimbus/log/logger.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

namespace nimbus
{
namespace render
{

namespace
{
const std::string logPrefix_ = "render.radar_sweep_layer";
const auto         logger_    = nimbus::log::Create(logPrefix_);

// Same mbgl-private constants as radar_site_marker_layer.cpp's MVP construction (see that file's
// top comment) - matches the legacy app's map::RadarProductLayer::Render exactly.
constexpr double kTileSize   = 512.0;
constexpr double kDegreesMax = 360.0;

void CheckGlError(QOpenGLFunctions_3_3_Core* gl, const char* where)
{
   for (GLenum err = gl->glGetError(); err != GL_NO_ERROR; err = gl->glGetError())
   {
      logger_->error("GL error {} at {}", static_cast<unsigned int>(err), where);
   }
}
} // namespace

class RadarSweepLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<products::RadarSweepProduct> product) :
       product_ {std::move(product)}
   {
   }

   ~Impl()
   {
      // ~QOpenGLShaderProgram deletes the GL program object, so like every other GL call it needs
      // the owning context to be current, and mbgl can destroy a custom layer host during Map
      // teardown - after the context is gone. deinitialize() guards the same hazard but is not
      // guaranteed to have run while a context was still current, so the destructor cannot rely
      // on it.
      //
      // NOTE: this is hardening against genuine undefined behaviour, *not* the fix for the
      // shutdown crash tracked in docs/ROADMAP.md - that crash was proven to happen with no
      // custom layer registered at all, so it does not originate here.
      if (QOpenGLContext::currentContext() == nullptr)
      {
         // Deliberately leaked: the context that owned these is gone, and it reclaimed the
         // underlying GL objects when it was destroyed. The process is exiting anyway.
         (void) shaderProgram_.release();
         (void) gl_.release();
      }
   }

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void UploadSweep(QOpenGLFunctions_3_3_Core*                gl,
                    const std::shared_ptr<const products::SweepData>& sweep);

   std::shared_ptr<products::RadarSweepProduct> product_;

   std::unique_ptr<QOpenGLShaderProgram>      shaderProgram_ {nullptr};
   std::unique_ptr<QOpenGLFunctions_3_3_Core> gl_ {nullptr};

   GLuint                vao_ {0};
   std::array<GLuint, 2> vbo_ {0, 0};
   GLuint                texture_ {0};

   int uMVPMatrixLocation_ {-1};
   int uOriginLatLongLocation_ {-1};
   int uDataMomentOffsetLocation_ {-1};
   int uDataMomentScaleLocation_ {-1};
   int uCFPEnabledLocation_ {-1};

   std::shared_ptr<const products::SweepData> lastUploaded_ {nullptr};

   GLsizei       numVertices_ {0};
   std::uint16_t colorTableMin_ {0};
   float         colorTableScale_ {1.0f};
};

void RadarSweepLayer::Impl::UploadSweep(QOpenGLFunctions_3_3_Core* gl,
                                        const std::shared_ptr<const products::SweepData>& sweep)
{
   gl->glBindVertexArray(vao_);

   gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_[0]);
   gl->glBufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(sweep->vertices.size() * sizeof(float)),
                    sweep->vertices.data(),
                    GL_STATIC_DRAW);
   gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
   gl->glEnableVertexAttribArray(0);

   const void* momentData {nullptr};
   GLsizeiptr  momentDataSize {0};
   GLenum      momentType {GL_UNSIGNED_BYTE};

   if (!sweep->dataMoments8.empty())
   {
      momentData     = sweep->dataMoments8.data();
      momentDataSize = static_cast<GLsizeiptr>(sweep->dataMoments8.size() * sizeof(std::uint8_t));
      momentType     = GL_UNSIGNED_BYTE;
   }
   else
   {
      momentData     = sweep->dataMoments16.data();
      momentDataSize =
         static_cast<GLsizeiptr>(sweep->dataMoments16.size() * sizeof(std::uint16_t));
      momentType = GL_UNSIGNED_SHORT;
   }

   gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_[1]);
   gl->glBufferData(GL_ARRAY_BUFFER, momentDataSize, momentData, GL_STATIC_DRAW);
   gl->glVertexAttribIPointer(1, 1, momentType, 0, nullptr);
   gl->glEnableVertexAttribArray(1);

   // No CFP data this slice (RadarSweepProduct doesn't compute it) - location 2 (aCfpMoment) is
   // simply never enabled, which the shader's uCFPEnabled=false uniform (set every render() call)
   // makes irrelevant, matching the legacy layer's own behavior when CFP data is absent.

   gl->glActiveTexture(GL_TEXTURE0);
   gl->glBindTexture(GL_TEXTURE_1D, texture_);
   gl->glTexImage1D(GL_TEXTURE_1D,
                    0,
                    GL_RGBA,
                    static_cast<GLsizei>(sweep->colorTableLut.size()),
                    0,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    sweep->colorTableLut.data());
   gl->glGenerateMipmap(GL_TEXTURE_1D);

   numVertices_     = static_cast<GLsizei>(sweep->vertices.size() / 2);
   colorTableMin_   = sweep->colorTableMin;
   colorTableScale_ = static_cast<float>(sweep->colorTableMax - sweep->colorTableMin);

   CheckGlError(gl, "UploadSweep()");
}

RadarSweepLayer::RadarSweepLayer(std::shared_ptr<products::RadarSweepProduct> product) :
    p {std::make_unique<Impl>(std::move(product))}
{
}

RadarSweepLayer::~RadarSweepLayer() = default;

void RadarSweepLayer::initialize()
{
   logger_->debug("initialize()");

   p->gl_ = std::make_unique<QOpenGLFunctions_3_3_Core>();
   if (!p->gl_->initializeOpenGLFunctions())
   {
      logger_->error("Failed to initialize OpenGL 3.3 core functions");
      return;
   }

   p->shaderProgram_ = std::make_unique<QOpenGLShaderProgram>();
   if (!p->shaderProgram_->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                                   ":/qt/qml/Nimbus/App/res/gl/radar.vert"))
   {
      logger_->error("Vertex shader compile failed: {}", p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                   ":/qt/qml/Nimbus/App/res/gl/radar.frag"))
   {
      logger_->error("Fragment shader compile failed: {}",
                     p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->link())
   {
      logger_->error("Shader link failed: {}", p->shaderProgram_->log().toStdString());
   }

   p->uMVPMatrixLocation_        = p->shaderProgram_->uniformLocation("uMVPMatrix");
   p->uOriginLatLongLocation_    = p->shaderProgram_->uniformLocation("uOriginLatLong");
   p->uDataMomentOffsetLocation_ = p->shaderProgram_->uniformLocation("uDataMomentOffset");
   p->uDataMomentScaleLocation_  = p->shaderProgram_->uniformLocation("uDataMomentScale");
   p->uCFPEnabledLocation_       = p->shaderProgram_->uniformLocation("uCFPEnabled");

   p->gl_->glGenVertexArrays(1, &p->vao_);
   p->gl_->glGenBuffers(2, p->vbo_.data());

   p->gl_->glGenTextures(1, &p->texture_);
   p->gl_->glBindTexture(GL_TEXTURE_1D, p->texture_);
   p->gl_->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   p->gl_->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   p->gl_->glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

   CheckGlError(p->gl_.get(), "initialize()");
}

void RadarSweepLayer::render(const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->shaderProgram_ || !p->shaderProgram_->isLinked() || !p->gl_ ||
       QOpenGLContext::currentContext() == nullptr)
   {
      // See deinitialize() for why a missing current context has to be checked, not assumed.
      return;
   }

   std::shared_ptr<const products::SweepData> sweep = p->product_->sweep_data();
   if (sweep == nullptr)
   {
      // No data loaded yet - nothing to draw.
      return;
   }

   if (sweep != p->lastUploaded_)
   {
      p->UploadSweep(p->gl_.get(), sweep);
      p->lastUploaded_ = sweep;
   }

   if (p->numVertices_ == 0)
   {
      return;
   }

   try
   {
      p->gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      p->shaderProgram_->bind();

      const double scale  = std::pow(2.0, params.zoom) * 2.0 * kTileSize / kDegreesMax;
      const auto   xScale = static_cast<float>(scale / params.width);
      const auto   yScale = static_cast<float>(scale / params.height);

      glm::mat4 mvpMatrix(1.0f);
      mvpMatrix = glm::scale(mvpMatrix, glm::vec3(xScale, yScale, 1.0f));
      mvpMatrix = glm::rotate(mvpMatrix,
                              glm::radians(static_cast<float>(params.bearing)),
                              glm::vec3(0.0f, 0.0f, 1.0f));

      p->gl_->glUniformMatrix4fv(
         p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
      p->gl_->glUniform2f(p->uOriginLatLongLocation_,
                          static_cast<float>(params.latitude),
                          static_cast<float>(params.longitude));
      p->gl_->glUniform1ui(p->uDataMomentOffsetLocation_, p->colorTableMin_);
      p->gl_->glUniform1f(p->uDataMomentScaleLocation_, p->colorTableScale_);
      p->gl_->glUniform1i(p->uCFPEnabledLocation_, 0);

      p->gl_->glActiveTexture(GL_TEXTURE0);
      p->gl_->glBindTexture(GL_TEXTURE_1D, p->texture_);
      p->gl_->glBindVertexArray(p->vao_);

      p->gl_->glDrawArrays(GL_TRIANGLES, 0, p->numVertices_);

      p->gl_->glBindVertexArray(0);
      p->shaderProgram_->release();

      CheckGlError(p->gl_.get(), "render()");
   }
   catch (const std::exception& ex)
   {
      logger_->error("Exception in render(): {}", ex.what());
   }
   catch (...)
   {
      logger_->error("Unknown exception in render()");
   }
}

void RadarSweepLayer::deinitialize()
{
   logger_->debug("deinitialize()");

   // Every QOpenGLFunctions_3_3_Core entry point dereferences a function-pointer table resolved
   // against the context that was current during initializeOpenGLFunctions(); calling one with no
   // current context is undefined. A layer can be torn down in exactly that state when its map is
   // destroyed along with the QML item that owned the context. QOpenGLShaderProgram's destructor
   // has the same requirement. (Hardening only - see ~Impl's note: this is not the cause of the
   // shutdown crash tracked in docs/ROADMAP.md.)
   if (QOpenGLContext::currentContext() == nullptr)
   {
      logger_->warn("deinitialize() with no current GL context - leaking GPU resources rather "
                    "than crashing; the context's own teardown reclaims them");
      // Deliberately leaked, not deleted: releasing them needs the context that is already gone.
      (void) p->shaderProgram_.release();
      p->gl_.reset();
      return;
   }

   if (p->gl_)
   {
      p->gl_->glDeleteVertexArrays(1, &p->vao_);
      p->gl_->glDeleteBuffers(static_cast<GLsizei>(p->vbo_.size()), p->vbo_.data());
      p->gl_->glDeleteTextures(1, &p->texture_);
   }
   p->shaderProgram_.reset();
   p->gl_.reset();
}

} // namespace render
} // namespace nimbus
