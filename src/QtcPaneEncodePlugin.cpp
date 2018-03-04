#include <QSettings>
#include <QTranslator>
#include <QDebug>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>
#include <projectexplorer/task.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildsteplist.h>
#include <extensionsystem/pluginmanager.h>
#include <projectexplorer/buildmanager.h>
#include <coreplugin/icore.h>
#include <coreplugin/ioutputpane.h>
#include <coreplugin/outputwindow.h>
#include <utils/outputformatter.h>

#include <QtPlugin>

#include "QtcPaneEncodePlugin.h"
#include "OptionsPage.h"
#include "Utils.h"
#include "Constants.h"

using namespace QtcPaneEncode::Internal;
using namespace QtcPaneEncode::Constants;

using namespace Core;
using namespace ProjectExplorer;
using namespace ExtensionSystem;

namespace {
  const QString appOutputPaneClassName =
    QLatin1String ("ProjectExplorer::Internal::AppOutputPane");
  const QString compileOutputWindowClassName =
    QLatin1String ("ProjectExplorer::Internal::CompileOutputWindow");
}

QtcPaneEncodePlugin::QtcPaneEncodePlugin () :
  IPlugin () {
  // Create your members
}

QtcPaneEncodePlugin::~QtcPaneEncodePlugin () {
  // Unregister objects from the plugin manager's object pool
  // Delete members
}

bool QtcPaneEncodePlugin::initialize (const QStringList &arguments, QString *errorString) {
  // Register objects in the plugin manager's object pool
  // Load settings
  // Add actions to menus
  // Connect to other plugins' signals
  // In the initialize function, a plugin can be sure that the plugins it
  // depends on have initialized their members.

  Q_UNUSED (arguments)
  Q_UNUSED (errorString)

  initLanguage ();
  updateSettings ();

  OptionsPage *optionsPage = new OptionsPage;
  connect (optionsPage, SIGNAL (settingsChanged ()), SLOT (updateSettings ()));
  addAutoReleasedObject (optionsPage);

  return true;
}

void QtcPaneEncodePlugin::initLanguage () {
  const QString &language = Core::ICore::userInterfaceLanguage ();
  if (!language.isEmpty ()) {
    QStringList paths;
    paths << ICore::resourcePath () << ICore::userResourcePath ();
    const QString &trFile = QLatin1String ("QtcPaneEncode_") + language;
    QTranslator *translator = new QTranslator (this);
    foreach (const QString &path, paths) {
      if (translator->load (trFile, path + QLatin1String ("/translations"))) {
        qApp->installTranslator (translator);
        break;
      }
    }
  }
}

void QtcPaneEncodePlugin::updateSettings () {
  Q_ASSERT (Core::ICore::settings () != nullptr);
  QSettings &settings = *(Core::ICore::settings ());
  settings.beginGroup (SETTINGS_GROUP);

  if (settings.value (SETTINGS_BUILD_ENABLED, false).toBool ()) {
    buildEncoding_ = settings.value (SETTINGS_BUILD_ENCODING, AUTO_ENCODING).toByteArray ();
  }
  else {
    buildEncoding_.clear ();
  }

  if (settings.value (SETTINGS_APP_ENABLED, false).toBool ()) {
    appEncoding_ = settings.value (SETTINGS_APP_ENCODING, AUTO_ENCODING).toByteArray ();
  }
  else {
    appEncoding_.clear ();
  }

  settings.endGroup ();
}

void QtcPaneEncodePlugin::extensionsInitialized () {
  // Retrieve objects from the plugin manager's object pool
  // In the extensionsInitialized function, a plugin can be sure that all
  // plugins that depend on it are completely initialized.

  // Compiler output
  QObject::connect(BuildManager::instance(), &BuildManager::buildStateChanged,
           this, &QtcPaneEncodePlugin::handleBuild);

  // These methods are private in BuildEngine, we cannot connect to them
  // connect (this, &QtcPaneEncodePlugin::newOutput,
  //         BuildManager::instance (), &BuildManager::addToOutputWindow);  
  // connect (this, &QtcPaneEncodePlugin::newTask,
  //         BuildManager::instance (), &BuildManager::addToTaskWindow);

  // connect to lambda instead and use other methods to produce output
  QObject::connect(this, &QtcPaneEncodePlugin::newOutput, []
                   (const QString &string, ProjectExplorer::BuildStep::OutputFormat format,
                   ProjectExplorer::BuildStep::OutputNewlineSetting newlineSetting) {
      Q_UNUSED(newlineSetting)

      // convert from BuildStep::OutputFormat to Utils::OutputFormat
      Utils::OutputFormat uformat = Utils::OutputFormat::NormalMessageFormat;
      switch (format) {
      case ProjectExplorer::BuildStep::OutputFormat::NormalMessage: uformat = Utils::OutputFormat::NormalMessageFormat; break;
      case ProjectExplorer::BuildStep::OutputFormat::ErrorMessage : uformat = Utils::OutputFormat::ErrorMessageFormat; break;
      case ProjectExplorer::BuildStep::OutputFormat::Stderr: uformat = Utils::OutputFormat::StdErrFormat; break;
      case ProjectExplorer::BuildStep::OutputFormat::Stdout: uformat = Utils::OutputFormat::StdOutFormat; break;
      }

      QObject *ocompileOutputWindow = PluginManager::getObjectByClassName (compileOutputWindowClassName);
      Core::IOutputPane *outputPane = qobject_cast<Core::IOutputPane *>(ocompileOutputWindow);
      if (outputPane) {
          Core::OutputWindow *outputWindow = qobject_cast<Core::OutputWindow *>(outputPane->outputWidget(nullptr));
          if (outputWindow) {
            Utils::OutputFormatter *formatter = outputWindow->formatter();
            if (formatter == nullptr) {
                qWarning() << Q_FUNC_INFO << "output pane has no formatter!";
                Utils::OutputFormatter *newFormatter = new Utils::OutputFormatter();
                outputWindow->setFormatter(newFormatter);
                // may cause a memory leak in CompilerOutputWindow?
                // it seems that output window does not delete formatter itself...
            }
            outputWindow->appendMessage(string, uformat); // this is not working if formatter is not set
            // outputWindow->appendPlainText(string); // this always works, but ignores formatting
          } else {
            qWarning() << Q_FUNC_INFO << "we wanted to output; but we can't find Core::OutputWindow :(" << string;
          }
      } else {
        qWarning() << Q_FUNC_INFO << "we wanted to output; but we can't find compile output pane :(" << string;
      }
  });

  // Run control output
  QObject *appOutputPane = PluginManager::getObjectByClassName (appOutputPaneClassName);
  if (appOutputPane != nullptr) {
    // TODO: no such signal anymore
    // QObject::connect: No such signal ProjectExplorer::ProjectExplorerPlugin::runControlStarted (ProjectExplorer::RunControl *)
    // QObject::connect(ProjectExplorerPlugin::instance(), SIGNAL (runControlStarted (ProjectExplorer::RunControl *)),
    //         this, SLOT (handleRunStart (ProjectExplorer::RunControl *)));

    QObject::connect(this, SIGNAL (newMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)),
                   appOutputPane, SLOT (appendMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)));
  }
  else {
    qCritical () << "Failed to find appOutputPane";
  }
}

ExtensionSystem::IPlugin::ShutdownFlag QtcPaneEncodePlugin::aboutToShutdown () {
  // Save settings
  // Disconnect from signals that are not needed during shutdown
  // Hide UI(if you add UI that is not in the main window directly)
  this->disconnect ();
  return SynchronousShutdown;
}

void QtcPaneEncodePlugin::handleBuild (ProjectExplorer::Project *project) {
  if (!BuildManager::isBuilding ()) {
    return;
  }

  const Target *buildingTarget = nullptr;
  for (ProjectExplorer::Target *target : project->targets()) {
    if (BuildManager::isBuilding (target)) {
      buildingTarget = target;
      break;
    }
  }
  if (buildingTarget == nullptr) {
    return;
  }

  BuildConfiguration *buildingConfiguration = nullptr;
  for (ProjectExplorer::BuildConfiguration *config : buildingTarget->buildConfigurations()) {
    if (BuildManager::isBuilding (config)) {
      buildingConfiguration = config;
      break;
    }
  }
  if (buildingConfiguration == nullptr) {
    return;
  }

  QList<Core::Id> stepsIds = buildingConfiguration->knownStepLists ();
  for (const Core::Id &id : stepsIds) {
    BuildStepList *steps = buildingConfiguration->stepList (id);
    if (steps == nullptr) {
      continue;
    }
    for (int i = 0, end = steps->count (); i < end; ++i) {
      BuildStep *step = steps->at (i);
      QObject::disconnect(step, &BuildStep::addOutput, nullptr, nullptr);
      QObject::connect(step, &BuildStep::addOutput, this, &QtcPaneEncodePlugin::addOutput, Qt::UniqueConnection);

      QObject::disconnect(step, &BuildStep::addTask, nullptr, nullptr);
      QObject::connect(step, &BuildStep::addTask, this, &QtcPaneEncodePlugin::addTask, Qt::UniqueConnection);
    }
  }
}

void QtcPaneEncodePlugin::addTask (const Task &task, int linkedOutputLines, int skipLines) {
  if (buildEncoding_.isEmpty ()) {
    emit newTask (task, linkedOutputLines, skipLines);
    return;
  }
  Task convertedTask = task;
  // Unknown charset will be handled like auto-detection request
  convertedTask.description = reencode (task.description,
                                        QTextCodec::codecForName (buildEncoding_));
  emit newTask (convertedTask, linkedOutputLines, skipLines);
}

void QtcPaneEncodePlugin::addOutput (const QString &string, BuildStep::OutputFormat format, BuildStep::OutputNewlineSetting newlineSetting) {
  if (buildEncoding_.isEmpty ()) {
    emit newOutput (string, format, newlineSetting);
    return;
  }
  QString convertedString = reencode (string,
                                      QTextCodec::codecForName (buildEncoding_));
  emit newOutput (convertedString, format, newlineSetting);
}

void QtcPaneEncodePlugin::handleRunStart (RunControl *runControl) {
  QObject *appOutputPane = PluginManager::getObjectByClassName (appOutputPaneClassName);
  if (appOutputPane != nullptr) {
    connect (runControl, SIGNAL (appendMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)),
             this, SLOT (appendMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)),
             Qt::UniqueConnection);
    disconnect (runControl, SIGNAL (appendMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)),
                appOutputPane, SLOT (appendMessage (ProjectExplorer::RunControl *,QString,Utils::OutputFormat)));
  }
}

void QtcPaneEncodePlugin::appendMessage (RunControl *rc, const QString &out, Utils::OutputFormat format) {
  if (appEncoding_.isEmpty ()) {
    emit newMessage (rc, out, format);
    return;
  }
  QString convertedOut = reencode (out, QTextCodec::codecForName (appEncoding_));
  emit newMessage (rc, convertedOut, format);
}
