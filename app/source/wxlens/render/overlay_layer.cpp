#include <wxlens/render/overlay_layer.hpp>
#include <wxlens/log/logger.hpp>

#include <cmath>
#include <cstddef>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

namespace wxlens
{
namespace render
{

namespace
{
const std::string logPrefix_ = "render.overlay_layer";
const auto        logger_    = wxlens::log::Create(logPrefix_);

// Same mbgl-private constants RadarSweepLayer's MVP construction uses (see that file).
constexpr double kTileSize   = 512.0;
constexpr double kDegreesMax = 360.0;

void CheckGlError(QOpenGLFunctions_3_3_Core* gl, const char* where)
{
   for (GLenum err = gl->glGetError(); err != GL_NO_ERROR; err = gl->glGetError())
   {
      logger_->error("GL error {} at {}", static_cast<unsigned int>(err), where);
   }
}

// See RadarSweepLayer's DrainPendingGlErrors: mbgl leaves its own errors queued mid-frame, and
// without draining on entry this layer would be blamed for them.
void DrainPendingGlErrors(QOpenGLFunctions_3_3_Core* gl, const char* where)
{
   for (GLenum err = gl->glGetError(); err != GL_NO_ERROR; err = gl->glGetError())
   {
      logger_->error("GL error {} already queued on entry to {} (not raised by this layer)",
                     static_cast<unsigned int>(err),
                     where);
   }
}
} // namespace

void OverlayLayerBinding::setData(std::shared_ptr<const OverlayGeometry> data)
{
   std::scoped_lock lock {mutex_};
   data_ = std::move(data);
}

std::shared_ptr<const OverlayGeometry> OverlayLayerBinding::data() const
{
   std::scoped_lock lock {mutex_};
   return data_;
}

class OverlayLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<OverlayLayerBinding> binding) : binding_ {std::move(binding)} {}

   // Same hazard and fix as RadarSweepLayer::Impl's destructor: mbgl can destroy a custom layer
   // host after its owning GL context is already gone.
   ~Impl()
   {
      if (QOpenGLContext::currentContext() == nullptr)
      {
         (void) shaderProgram_.release();
         (void) gl_.release();
      }
   }

   Impl(const Impl&)            = delete;
   Impl& operator=(const Impl&) = delete;
   Impl(Impl&&)                 = delete;
   Impl& operator=(Impl&&)      = delete;

   void Upload(const std::shared_ptr<const OverlayGeometry>& data);

   std::shared_ptr<OverlayLayerBinding> binding_;

   std::unique_ptr<QOpenGLShaderProgram>      shaderProgram_ {nullptr};
   std::unique_ptr<QOpenGLFunctions_3_3_Core> gl_ {nullptr};

   GLuint vao_ {0};
   GLuint vbo_ {0};

   int uMVPMatrixLocation_     {-1};
   int uOriginLatLongLocation_ {-1};
   int uViewportHalfLocation_  {-1};

   std::shared_ptr<const OverlayGeometry> lastUploaded_ {nullptr};
   GLsizei                                numVertices_ {0};
};

void OverlayLayer::Impl::Upload(const std::shared_ptr<const OverlayGeometry>& data)
{
   auto* gl = gl_.get();
   DrainPendingGlErrors(gl, "Upload()");

   gl->glBindVertexArray(vao_);
   gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_);
   gl->glBufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(data->vertices.size() * sizeof(OverlayVertex)),
                    data->vertices.data(),
                    GL_STATIC_DRAW);

   const auto stride = static_cast<GLsizei>(sizeof(OverlayVertex));
   const auto offset = [](std::size_t bytes) { return reinterpret_cast<void*>(bytes); };

   // location 0: anchor lat/lon, 1: other segment end lat/lon, 2: (side, halfWidthPx), 3: color
   gl->glVertexAttribPointer(
      0, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(OverlayVertex, latitude)));
   gl->glEnableVertexAttribArray(0);
   gl->glVertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(OverlayVertex, otherLatitude)));
   gl->glEnableVertexAttribArray(1);
   gl->glVertexAttribPointer(
      2, 2, GL_FLOAT, GL_FALSE, stride, offset(offsetof(OverlayVertex, side)));
   gl->glEnableVertexAttribArray(2);
   gl->glVertexAttribPointer(
      3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, offset(offsetof(OverlayVertex, r)));
   gl->glEnableVertexAttribArray(3);

   gl->glBindVertexArray(0);

   numVertices_ = static_cast<GLsizei>(data->vertices.size());
   CheckGlError(gl, "Upload()");
}

OverlayLayer::OverlayLayer(std::shared_ptr<OverlayLayerBinding> binding) :
    p {std::make_unique<Impl>(std::move(binding))}
{
}

OverlayLayer::~OverlayLayer() = default;

void OverlayLayer::initialize()
{
   logger_->debug("initialize()");

   p->gl_ = std::make_unique<QOpenGLFunctions_3_3_Core>();
   if (!p->gl_->initializeOpenGLFunctions())
   {
      logger_->error("Failed to initialize OpenGL 3.3 core functions");
      return;
   }
   DrainPendingGlErrors(p->gl_.get(), "initialize()");

   p->shaderProgram_ = std::make_unique<QOpenGLShaderProgram>();
   if (!p->shaderProgram_->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                                   ":/qt/qml/WxLens/App/res/gl/overlay.vert"))
   {
      logger_->error("Vertex shader compile failed: {}", p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                   ":/qt/qml/WxLens/App/res/gl/overlay.frag"))
   {
      logger_->error("Fragment shader compile failed: {}",
                     p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->link())
   {
      logger_->error("Shader link failed: {}", p->shaderProgram_->log().toStdString());
   }

   p->uMVPMatrixLocation_     = p->shaderProgram_->uniformLocation("uMVPMatrix");
   p->uOriginLatLongLocation_ = p->shaderProgram_->uniformLocation("uOriginLatLong");
   p->uViewportHalfLocation_  = p->shaderProgram_->uniformLocation("uViewportHalf");

   p->gl_->glGenVertexArrays(1, &p->vao_);
   p->gl_->glGenBuffers(1, &p->vbo_);

   CheckGlError(p->gl_.get(), "initialize()");
}

void OverlayLayer::render(const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->shaderProgram_ || !p->shaderProgram_->isLinked() || !p->gl_ ||
       QOpenGLContext::currentContext() == nullptr)
   {
      return;
   }

   const std::shared_ptr<const OverlayGeometry> data = p->binding_->data();
   if (data == nullptr || data->vertices.empty())
   {
      return;
   }

   try
   {
      if (data != p->lastUploaded_)
      {
         p->Upload(data);
         p->lastUploaded_ = data;
      }

      DrainPendingGlErrors(p->gl_.get(), "render()");
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

      p->gl_->glUniformMatrix4fv(p->uMVPMatrixLocation_, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
      p->gl_->glUniform2f(p->uOriginLatLongLocation_,
                          static_cast<float>(params.latitude),
                          static_cast<float>(params.longitude));
      // The same logical-pixel viewport the MVP above is built from, so line widths come out in
      // the logical pixels the old Canvas used.
      p->gl_->glUniform2f(p->uViewportHalfLocation_,
                          static_cast<float>(params.width * 0.5),
                          static_cast<float>(params.height * 0.5));

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

void OverlayLayer::deinitialize()
{
   logger_->debug("deinitialize()");

   // Same hazard and fix as RadarSweepLayer::deinitialize.
   if (QOpenGLContext::currentContext() == nullptr)
   {
      logger_->warn("deinitialize() with no current GL context - leaking GPU resources rather "
                    "than crashing; the context's own teardown reclaims them");
      (void) p->shaderProgram_.release();
      p->gl_.reset();
      return;
   }

   if (p->gl_)
   {
      p->gl_->glDeleteVertexArrays(1, &p->vao_);
      p->gl_->glDeleteBuffers(1, &p->vbo_);
   }
   p->shaderProgram_.reset();
   p->gl_.reset();
   p->lastUploaded_.reset();
   p->numVertices_ = 0;
}

} // namespace render
} // namespace wxlens
