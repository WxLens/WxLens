#include <wxlens/render/polyline_layer.hpp>
#include <wxlens/log/logger.hpp>

#include <cmath>
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
const std::string logPrefix_ = "render.polyline_layer";
const auto         logger_    = wxlens::log::Create(logPrefix_);

// Same mbgl-private constants RadarSweepLayer's MVP construction uses (see that file's comment).
constexpr double kTileSize   = 512.0;
constexpr double kDegreesMax = 360.0;

// Deliberately uniform, not per-item (placefile Line items carry their own width_, alert polygons
// could vary by threat category) - core-profile GL_LINES width beyond 1px is unreliably supported
// across drivers (many silently clamp glLineWidth to 1.0), so per-item width would be a value
// that looks different machine to machine. A geometry-expanded thick line is the correct fix and
// is exactly the kind of follow-up work real per-item styling would deliver.
constexpr float kLineWidth = 1.5f;

void CheckGlError(QOpenGLFunctions_3_3_Core* gl, const char* where)
{
   for (GLenum err = gl->glGetError(); err != GL_NO_ERROR; err = gl->glGetError())
   {
      logger_->error("GL error {} at {}", static_cast<unsigned int>(err), where);
   }
}
} // namespace

void PolylineLayerBinding::setData(std::shared_ptr<const PolylineData> data)
{
   std::scoped_lock lock {mutex_};
   data_ = std::move(data);
}

std::shared_ptr<const PolylineData> PolylineLayerBinding::data() const
{
   std::scoped_lock lock {mutex_};
   return data_;
}

class PolylineLayer::Impl
{
public:
   explicit Impl(std::shared_ptr<PolylineLayerBinding> binding) : binding_ {std::move(binding)} {}

   // See RadarSweepLayer::Impl's ~Impl for why this guard exists - identical hazard, identical
   // fix: mbgl can destroy a custom layer host after its owning GL context is already gone.
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

   void Upload(QOpenGLFunctions_3_3_Core* gl, const std::shared_ptr<const PolylineData>& data);

   std::shared_ptr<PolylineLayerBinding> binding_;

   std::unique_ptr<QOpenGLShaderProgram>      shaderProgram_ {nullptr};
   std::unique_ptr<QOpenGLFunctions_3_3_Core> gl_ {nullptr};

   GLuint vao_ {0};
   GLuint vbo_ {0};

   int uMVPMatrixLocation_     {-1};
   int uOriginLatLongLocation_ {-1};

   std::shared_ptr<const PolylineData> lastUploaded_ {nullptr};
   GLsizei                             numVertices_ {0};
};

void PolylineLayer::Impl::Upload(QOpenGLFunctions_3_3_Core*                gl,
                                 const std::shared_ptr<const PolylineData>& data)
{
   gl->glBindVertexArray(vao_);

   gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_);
   gl->glBufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(data->vertices.size() * sizeof(PolylineVertex)),
                    data->vertices.data(),
                    GL_STATIC_DRAW);

   gl->glVertexAttribPointer(0,
                             2,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(PolylineVertex),
                             reinterpret_cast<void*>(offsetof(PolylineVertex, latitude)));
   gl->glEnableVertexAttribArray(0);

   gl->glVertexAttribPointer(1,
                             4,
                             GL_FLOAT,
                             GL_FALSE,
                             sizeof(PolylineVertex),
                             reinterpret_cast<void*>(offsetof(PolylineVertex, r)));
   gl->glEnableVertexAttribArray(1);

   numVertices_ = static_cast<GLsizei>(data->vertices.size());

   CheckGlError(gl, "Upload()");
}

PolylineLayer::PolylineLayer(std::shared_ptr<PolylineLayerBinding> binding) :
    p {std::make_unique<Impl>(std::move(binding))}
{
}

PolylineLayer::~PolylineLayer() = default;

void PolylineLayer::initialize()
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
                                                   ":/qt/qml/WxLens/App/res/gl/polyline.vert"))
   {
      logger_->error("Vertex shader compile failed: {}", p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                                   ":/qt/qml/WxLens/App/res/gl/polyline.frag"))
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

   p->gl_->glGenVertexArrays(1, &p->vao_);
   p->gl_->glGenBuffers(1, &p->vbo_);

   CheckGlError(p->gl_.get(), "initialize()");
}

void PolylineLayer::render(const QMapLibre::CustomLayerRenderParameters& params)
{
   if (!p->shaderProgram_ || !p->shaderProgram_->isLinked() || !p->gl_ ||
       QOpenGLContext::currentContext() == nullptr)
   {
      return;
   }

   std::shared_ptr<const PolylineData> data = p->binding_->data();
   if (data == nullptr)
   {
      return;
   }

   if (data != p->lastUploaded_)
   {
      p->Upload(p->gl_.get(), data);
      p->lastUploaded_ = data;
   }

   if (p->numVertices_ == 0)
   {
      return;
   }

   try
   {
      p->gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      p->gl_->glLineWidth(kLineWidth);

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

      p->gl_->glBindVertexArray(p->vao_);
      p->gl_->glDrawArrays(GL_LINES, 0, p->numVertices_);
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

void PolylineLayer::deinitialize()
{
   logger_->debug("deinitialize()");

   // See RadarSweepLayer::deinitialize's identical comment - same hazard, same fix.
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
}

} // namespace render
} // namespace wxlens
