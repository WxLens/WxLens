#include <nimbus/render/radar_site_marker_layer.hpp>
#include <nimbus/log/logger.hpp>

#include <array>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>

namespace nimbus
{
namespace render
{

namespace
{
const std::string logPrefix_ = "render.radar_site_marker_layer";
const auto         logger_    = nimbus::log::Create(logPrefix_);

// Standard Web Mercator constants matching the legacy RadarProductLayer::Render's MVP
// construction (scwx-qt/source/scwx/qt/map/radar_product_layer.cpp), which pulls them from
// mbgl::util::tileSize_D/DEGREES_MAX - a private MapLibre Native header, not part of the public
// API surface Nimbus depends on, hence hardcoded here instead.
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

class RadarSiteMarkerLayer::Impl
{
public:
   Impl(double latitude, double longitude) : latitude_ {latitude}, longitude_ {longitude} {}

   double latitude_;
   double longitude_;

   std::unique_ptr<QOpenGLShaderProgram>      shaderProgram_ {nullptr};
   std::unique_ptr<QOpenGLFunctions_3_3_Core> gl_ {nullptr};

   GLuint vao_ {0};
   GLuint vbo_ {0};

   int uMVPMatrixLocation_ {-1};
   int uOriginLatLongLocation_ {-1};
};

RadarSiteMarkerLayer::RadarSiteMarkerLayer(double latitude, double longitude) :
    p {std::make_unique<Impl>(latitude, longitude)}
{
}

RadarSiteMarkerLayer::~RadarSiteMarkerLayer() = default;

void RadarSiteMarkerLayer::initialize()
{
   logger_->debug("initialize()");

   p->gl_ = std::make_unique<QOpenGLFunctions_3_3_Core>();
   if (!p->gl_->initializeOpenGLFunctions())
   {
      logger_->error("Failed to initialize OpenGL 3.3 core functions");
      return;
   }

   p->shaderProgram_ = std::make_unique<QOpenGLShaderProgram>();
   if (!p->shaderProgram_->addShaderFromSourceFile(
          QOpenGLShader::Vertex, ":/qt/qml/Nimbus/App/res/gl/site_marker.vert"))
   {
      logger_->error("Vertex shader compile failed: {}",
                     p->shaderProgram_->log().toStdString());
   }
   if (!p->shaderProgram_->addShaderFromSourceFile(
          QOpenGLShader::Fragment, ":/qt/qml/Nimbus/App/res/gl/site_marker.frag"))
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

   if (p->uMVPMatrixLocation_ < 0 || p->uOriginLatLongLocation_ < 0)
   {
      logger_->error("Uniform location lookup failed: uMVPMatrix={}, uOriginLatLong={}",
                     p->uMVPMatrixLocation_,
                     p->uOriginLatLongLocation_);
   }

   p->gl_->glGenVertexArrays(1, &p->vao_);
   p->gl_->glGenBuffers(1, &p->vbo_);

   // aLatLong expects (latitude, longitude), matching radar.vert's convention.
   const std::array<float, 2> vertex {static_cast<float>(p->latitude_),
                                      static_cast<float>(p->longitude_)};

   p->gl_->glBindVertexArray(p->vao_);
   p->gl_->glBindBuffer(GL_ARRAY_BUFFER, p->vbo_);
   p->gl_->glBufferData(
      GL_ARRAY_BUFFER, sizeof(vertex), vertex.data(), GL_STATIC_DRAW);
   p->gl_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
   p->gl_->glEnableVertexAttribArray(0);

   CheckGlError(p->gl_.get(), "initialize()");

   logger_->info(
      "Radar site marker layer initialized at {}, {}", p->latitude_, p->longitude_);
}

void RadarSiteMarkerLayer::render(const QMapLibre::CustomLayerRenderParameters& params)
{
   static bool loggedFirstCall = false;
   if (!loggedFirstCall)
   {
      loggedFirstCall = true;
      logger_->info("render() called for the first time (zoom={}, w={}, h={})",
                    params.zoom,
                    params.width,
                    params.height);
   }

   if (!p->shaderProgram_ || !p->shaderProgram_->isLinked() || !p->gl_)
   {
      static bool loggedSkip = false;
      if (!loggedSkip)
      {
         loggedSkip = true;
         logger_->error("render() skipped - shaderProgram_={}, isLinked={}, gl_={}",
                        static_cast<const void*>(p->shaderProgram_.get()),
                        p->shaderProgram_ ? p->shaderProgram_->isLinked() : false,
                        static_cast<const void*>(p->gl_.get()));
      }
      return;
   }

   try
   {
      p->gl_->glEnable(GL_PROGRAM_POINT_SIZE);
      CheckGlError(p->gl_.get(), "render() glEnable");
      p->gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      CheckGlError(p->gl_.get(), "render() glBlendFunc");

      p->shaderProgram_->bind();
      CheckGlError(p->gl_.get(), "render() bind");

      // Matches the legacy RadarProductLayer::Render's MVP construction exactly (see this
      // file's top comment on kTileSize/kDegreesMax).
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
      CheckGlError(p->gl_.get(), "render() uMVPMatrix");
      p->gl_->glUniform2f(p->uOriginLatLongLocation_,
                          static_cast<float>(params.latitude),
                          static_cast<float>(params.longitude));
      CheckGlError(p->gl_.get(), "render() uOriginLatLong");

      p->gl_->glBindVertexArray(p->vao_);
      CheckGlError(p->gl_.get(), "render() bindVAO");
      p->gl_->glDrawArrays(GL_POINTS, 0, 1);
      CheckGlError(p->gl_.get(), "render() drawArrays");

      // Not strictly required - mbgl calls gl::Context::resetState() before every custom-layer
      // render() and setDirtyState() after (see docs/adr/0004-maplibre-qml-integration.md's
      // slice 3 notes), so it doesn't assume anything about GL state a host leaves behind.
      // Restoring anyway is cheap and defensive. (The actual black-map bug this was first
      // suspected of causing turned out to be unrelated - a framebuffer clear inside
      // QtOpenGLRenderableResource::bind(), fixed by patch 0006.)
      p->gl_->glBindVertexArray(0);
      p->shaderProgram_->release();
      p->gl_->glDisable(GL_PROGRAM_POINT_SIZE);

      CheckGlError(p->gl_.get(), "render() cleanup");
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

void RadarSiteMarkerLayer::deinitialize()
{
   logger_->debug("deinitialize()");

   if (p->gl_)
   {
      p->gl_->glDeleteVertexArrays(1, &p->vao_);
      p->gl_->glDeleteBuffers(1, &p->vbo_);
   }
   p->shaderProgram_.reset();
   p->gl_.reset();
}

} // namespace render
} // namespace nimbus
