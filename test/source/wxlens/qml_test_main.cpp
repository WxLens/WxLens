// SPDX-License-Identifier: MIT
//
// Entry point for WxLens's headless QML tests (ROADMAP slice 19).
//
// These exercise the logic that lives in QML and therefore cannot be reached by wxlens-app-test:
// filter predicates, guard flags and the handlers that maintain them. They deliberately do NOT
// verify rendering. The platform plugin is `offscreen` and, in CI, software-rasterized GL, so
// comparing pixels here would be comparing against something no user runs.
//
// QUICK_TEST_MAIN_WITH_SETUP is used rather than QUICK_TEST_MAIN because the application's C++
// models reach QML as context properties injected by main.cpp, not as registered QML types, so a
// test engine needs the same opportunity to populate a context.
#include <QtQuickTest>
#include <QQmlEngine>
#include <QObject>

class Setup : public QObject
{
   Q_OBJECT

public slots:
   void qmlEngineAvailable(QQmlEngine* engine)
   {
      // Individual tests inject their own mocks as component properties. Nothing global is
      // registered here yet; when a test needs a real wxlens model, add it to this context so
      // the test file's `import WxLens.App` sees the same shape the application does.
      Q_UNUSED(engine)
   }
};

QUICK_TEST_MAIN_WITH_SETUP(wxlens_qml, Setup)

#include "qml_test_main.moc"
