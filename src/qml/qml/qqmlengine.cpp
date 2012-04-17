/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qqmlengine_p.h"
#include "qqmlengine.h"
#include "qqmlcomponentattached_p.h"

#include "qqmlcontext_p.h"
#include "qqmlcompiler_p.h"
#include "qqml.h"
#include "qqmlcontext.h"
#include "qqmlexpression.h"
#include "qqmlcomponent.h"
#include "qqmlvme_p.h"
#include <private/qqmlenginedebugservice_p.h>
#include "qqmlstringconverters_p.h"
#include "qqmlxmlhttprequest_p.h"
#include "qqmlscriptstring.h"
#include "qqmlglobal_p.h"
#include "qquicklistmodel_p.h"
#include "qquickworkerscript_p.h"
#include "qqmlcomponent_p.h"
#include "qqmlnetworkaccessmanagerfactory.h"
#include "qqmldirparser_p.h"
#include "qqmlextensioninterface.h"
#include "qqmllist_p.h"
#include "qqmltypenamecache_p.h"
#include "qqmlnotifier_p.h"
#include <private/qqmlprofilerservice_p.h>
#include <private/qv8debugservice_p.h>
#include <private/qdebugmessageservice_p.h>
#include "qqmlincubator.h"
#include <private/qv8profilerservice_p.h>
#include <private/qqmlboundsignal_p.h>

#include <QtCore/qstandardpaths.h>
#include <QtCore/qsettings.h>

#include <QtCore/qmetaobject.h>
#include <QNetworkAccessManager>
#include <QDebug>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdir.h>
#include <QtCore/qmutex.h>
#include <QtNetwork/qnetworkconfigmanager.h>

#include <private/qobject_p.h>
#include <private/qmetaobject_p.h>

#include <private/qqmllocale_p.h>

#ifdef Q_OS_WIN // for %APPDATA%
#include <qt_windows.h>
#include <qlibrary.h>
#include <windows.h>

#define CSIDL_APPDATA		0x001a	// <username>\Application Data
#endif

Q_DECLARE_METATYPE(QQmlProperty)

QT_BEGIN_NAMESPACE

void qmlRegisterBaseTypes(const char *uri, int versionMajor, int versionMinor)
{
    QQmlEnginePrivate::registerBaseTypes(uri, versionMajor, versionMinor);
    QQmlValueTypeFactory::registerBaseTypes(uri, versionMajor, versionMinor);
}

/*!
  \qmlclass QtObject QObject
  \ingroup qml-utility-elements
  \since 4.7
  \brief The QtObject element is the most basic element in QML.

  The QtObject element is a non-visual element which contains only the
  objectName property.

  It can be useful to create a QtObject if you need an extremely
  lightweight element to enclose a set of custom properties:

  \snippet doc/src/snippets/qml/qtobject.qml 0

  It can also be useful for C++ integration, as it is just a plain
  QObject. See the QObject documentation for further details.
*/
/*!
  \qmlproperty string QtObject::objectName
  This property holds the QObject::objectName for this specific object instance.

  This allows a C++ application to locate an item within a QML component
  using the QObject::findChild() method. For example, the following C++
  application locates the child \l Rectangle item and dynamically changes its
  \c color value:

    \qml
    // MyRect.qml

    import QtQuick 2.0

    Item {
        width: 200; height: 200

        Rectangle {
            anchors.fill: parent
            color: "red"
            objectName: "myRect"
        }
    }
    \endqml

    \code
    // main.cpp

    QQuickView view;
    view.setSource(QUrl::fromLocalFile("MyRect.qml"));
    view.show();

    QQuickItem *item = view.rootObject()->findChild<QQuickItem*>("myRect");
    if (item)
        item->setProperty("color", QColor(Qt::yellow));
    \endcode
*/

bool QQmlEnginePrivate::qml_debugging_enabled = false;

void QQmlEnginePrivate::registerBaseTypes(const char *uri, int versionMajor, int versionMinor)
{
    qmlRegisterType<QQmlComponent>(uri,versionMajor,versionMinor,"Component");
    qmlRegisterType<QObject>(uri,versionMajor,versionMinor,"QtObject");
    qmlRegisterType<QQuickListElement>(uri, versionMajor, versionMinor,"ListElement");
    qmlRegisterCustomType<QQuickListModel>(uri, versionMajor, versionMinor,"ListModel", new QQuickListModelParser);
    qmlRegisterType<QQuickWorkerScript>(uri,versionMajor,versionMinor,"WorkerScript");
}

void QQmlEnginePrivate::defineModule()
{
    registerBaseTypes("QtQuick", 2, 0);
    qmlRegisterUncreatableType<QQmlLocale>("QtQuick",2,0,"Locale",QQmlEngine::tr("Locale cannot be instantiated.  Use Qt.locale()"));
}


/*!
    \class QQmlImageProviderBase
    \brief The QQmlImageProviderBase class is used to register image providers in the QML engine.
    \mainclass

    Image providers must be registered with the QML engine.  The only information the QML
    engine knows about image providers is the type of image data they provide.  To use an
    image provider to acquire image data, you must cast the QQmlImageProviderBase pointer
    to a QQuickImageProvider pointer.

    \sa QQuickImageProvider, QQuickTextureFactory
*/

/*!
    \enum QQmlImageProviderBase::ImageType

    Defines the type of image supported by this image provider.

    \value Image The Image Provider provides QImage images.
        The QQuickImageProvider::requestImage() method will be called for all image requests.
    \value Pixmap The Image Provider provides QPixmap images.
        The QQuickImageProvider::requestPixmap() method will be called for all image requests.
    \value Texture The Image Provider provides QSGTextureProvider based images.
        The QQuickImageProvider::requestTexture() method will be called for all image requests. \omitvalue
*/

/*! \internal */
QQmlImageProviderBase::QQmlImageProviderBase()
{
}

/*! \internal */
QQmlImageProviderBase::~QQmlImageProviderBase()
{
}


/*!
\qmlclass Qt QQmlEnginePrivate
  \ingroup qml-utility-elements
\brief The QML global Qt object provides useful enums and functions from Qt.

\keyword QmlGlobalQtObject

\brief The \c Qt object provides useful enums and functions from Qt, for use in all QML files.

The \c Qt object is a global object with utility functions, properties and enums.

It is not instantiable; to use it, call the members of the global \c Qt object directly.
For example:

\qml
import QtQuick 2.0

Text {
    color: Qt.rgba(1, 0, 0, 1)
    text: Qt.md5("hello, world")
}
\endqml


\section1 Enums

The Qt object contains the enums available in the \l {Qt Namespace}. For example, you can access
the \l Qt::LeftButton and \l Qt::RightButton enum values as \c Qt.LeftButton and \c Qt.RightButton.


\section1 Types
The Qt object also contains helper functions for creating objects of specific
data types. This is primarily useful when setting the properties of an item
when the property has one of the following types:

\list
\li \c color - use \l{Qt::rgba()}{Qt.rgba()}, \l{Qt::hsla()}{Qt.hsla()}, \l{Qt::darker()}{Qt.darker()}, \l{Qt::lighter()}{Qt.lighter()} or \l{Qt::tint()}{Qt.tint()}
\li \c rect - use \l{Qt::rect()}{Qt.rect()}
\li \c point - use \l{Qt::point()}{Qt.point()}
\li \c size - use \l{Qt::size()}{Qt.size()}
\li \c vector3d - use \l{Qt::vector3d()}{Qt.vector3d()}
\endlist

There are also string based constructors for these types. See \l{qdeclarativebasictypes.html}{QML Basic Types} for more information.

\section1 Date/Time Formatters

The Qt object contains several functions for formatting QDateTime, QDate and QTime values.

\list
    \li \l{Qt::formatDateTime}{string Qt.formatDateTime(datetime date, variant format)}
    \li \l{Qt::formatDate}{string Qt.formatDate(datetime date, variant format)}
    \li \l{Qt::formatTime}{string Qt.formatTime(datetime date, variant format)}
\endlist

The format specification is described at \l{Qt::formatDateTime}{Qt.formatDateTime}.


\section1 Dynamic Object Creation
The following functions on the global object allow you to dynamically create QML
items from files or strings. See \l{Dynamic Object Management in QML} for an overview
of their use.

\list
    \li \l{Qt::createComponent()}{object Qt.createComponent(url)}
    \li \l{Qt::createQmlObject()}{object Qt.createQmlObject(string qml, object parent, string filepath)}
\endlist
*/


/*!
    \qmlproperty object Qt::application
    \since QtQuick 1.1

    The \c application object provides access to global application state
    properties shared by many QML components.

    Its properties are:

    \table
    \row
    \li \c application.active
    \li
    This read-only property indicates whether the application is the top-most and focused
    application, and the user is able to interact with the application. The property
    is false when the application is in the background, the device keylock or screen
    saver is active, the screen backlight is turned off, or the global system dialog
    is being displayed on top of the application. It can be used for stopping and
    pausing animations, timers and active processing of data in order to save device
    battery power and free device memory and processor load when the application is not
    active.

    \row
    \li \c application.layoutDirection
    \li
    This read-only property can be used to query the default layout direction of the
    application. On system start-up, the default layout direction depends on the
    application's language. The property has a value of \c Qt.RightToLeft in locales
    where text and graphic elements are read from right to left, and \c Qt.LeftToRight
    where the reading direction flows from left to right. You can bind to this
    property to customize your application layouts to support both layout directions.

    Possible values are:

    \list
    \li Qt.LeftToRight - Text and graphics elements should be positioned
                        from left to right.
    \li Qt.RightToLeft - Text and graphics elements should be positioned
                        from right to left.
    \endlist

    The following example uses the \c application object to indicate
    whether the application is currently active:

    \snippet doc/src/snippets/qml/application.qml document

    \qmlproperty object Qt::inputMethod
    \since QtQuick 2.0

    The \c inputMethod object allows access to application's QInputMethod object
    and all its properties and slots. See the QInputMethod documentation for
    further details.
*/


/*!
\qmlmethod object Qt::include(string url, jsobject callback)

Includes another JavaScript file. This method can only be used from within JavaScript files,
and not regular QML files.

This imports all functions from \a url into the current script's namespace.

Qt.include() returns an object that describes the status of the operation.  The object has
a single property, \c {status}, that is set to one of the following values:

\table
\header \li Symbol \li Value \li Description
\row \li result.OK \li 0 \li The include completed successfully.
\row \li result.LOADING \li 1 \li Data is being loaded from the network.
\row \li result.NETWORK_ERROR \li 2 \li A network error occurred while fetching the url.
\row \li result.EXCEPTION \li 3 \li A JavaScript exception occurred while executing the included code.
An additional \c exception property will be set in this case.
\endtable

The \c status property will be updated as the operation progresses.

If provided, \a callback is invoked when the operation completes.  The callback is passed
the same object as is returned from the Qt.include() call.
*/
// Qt.include() is implemented in qv8include.cpp


QQmlEnginePrivate::QQmlEnginePrivate(QQmlEngine *e)
: propertyCapture(0), rootContext(0), isDebugging(false),
  outputWarningsToStdErr(true), sharedContext(0), sharedScope(0),
  cleanup(0), erroredBindings(0), inProgressCreations(0),
  workerScriptEngine(0), activeVME(0),
  networkAccessManager(0), networkAccessManagerFactory(0),
  scarceResourcesRefCount(0), typeLoader(e), importDatabase(e), uniqueId(1),
  incubatorCount(0), incubationController(0), mutex(QMutex::Recursive)
{
}

QQmlEnginePrivate::~QQmlEnginePrivate()
{
    if (inProgressCreations)
        qWarning() << QQmlEngine::tr("There are still \"%1\" items in the process of being created at engine destruction.").arg(inProgressCreations);

    while (cleanup) {
        QQmlCleanup *c = cleanup;
        cleanup = c->next;
        if (cleanup) cleanup->prev = &cleanup;
        c->next = 0;
        c->prev = 0;
        c->clear();
    }

    doDeleteInEngineThread();

    if (incubationController) incubationController->d = 0;
    incubationController = 0;

    delete rootContext;
    rootContext = 0;

    for(QHash<int, QQmlCompiledData*>::ConstIterator iter = m_compositeTypes.constBegin(); iter != m_compositeTypes.constEnd(); ++iter)
        (*iter)->release();
    for(QHash<const QMetaObject *, QQmlPropertyCache *>::Iterator iter = propertyCache.begin(); iter != propertyCache.end(); ++iter)
        (*iter)->release();
    for(QHash<QPair<QQmlType *, int>, QQmlPropertyCache *>::Iterator iter = typePropertyCache.begin(); iter != typePropertyCache.end(); ++iter)
        (*iter)->release();
    for(QHash<QQmlMetaType::ModuleApi, QQmlMetaType::ModuleApiInstance *>::Iterator iter = moduleApiInstances.begin(); iter != moduleApiInstances.end(); ++iter) {
        delete (*iter)->qobjectApi;
        delete *iter;
    }
}

void QQmlPrivate::qdeclarativeelement_destructor(QObject *o)
{
    QObjectPrivate *p = QObjectPrivate::get(o);
    if (p->declarativeData) {
        QQmlData *d = static_cast<QQmlData*>(p->declarativeData);
        if (d->ownContext && d->context) {
            d->context->destroy();
            d->context = 0;
        }

        // Mark this object as in the process of deletion to
        // prevent it resolving in bindings
        QQmlData::markAsDeleted(o);
    }
}

void QQmlData::destroyed(QAbstractDeclarativeData *d, QObject *o)
{
    static_cast<QQmlData *>(d)->destroyed(o);
}

void QQmlData::parentChanged(QAbstractDeclarativeData *d, QObject *o, QObject *p)
{
    static_cast<QQmlData *>(d)->parentChanged(o, p);
}

void QQmlData::signalEmitted(QAbstractDeclarativeData *, QObject *object, int index, void **a)
{
    QQmlData *ddata = QQmlData::get(object, false);
    if (!ddata) return; // Probably being deleted

    QQmlNotifierEndpoint *ep = ddata->notify(index);
    if (ep) QQmlNotifier::emitNotify(ep, a);
}

int QQmlData::receivers(QAbstractDeclarativeData *d, const QObject *, int index)
{
    return static_cast<QQmlData *>(d)->endpointCount(index);
}

int QQmlData::endpointCount(int index)
{
    int count = 0;
    QQmlNotifierEndpoint *ep = notify(index);
    if (!ep)
        return count;
    ++count;
    while (ep->next) {
        ++count;
        ep = ep->next;
    }
    return count;
}

void QQmlData::markAsDeleted(QObject *o)
{
    QQmlData::setQueuedForDeletion(o);

    QObjectPrivate *p = QObjectPrivate::get(o);
    for (QList<QObject *>::iterator it = p->children.begin(), end = p->children.end(); it != end; ++it) {
        QQmlData::markAsDeleted(*it);
    }
}

void QQmlEnginePrivate::init()
{
    Q_Q(QQmlEngine);

    static bool firstTime = true;
    if (firstTime) {
        qmlRegisterType<QQmlComponent>("QML", 1, 0, "Component");

        QQmlData::init();
        firstTime = false;
    }

    qRegisterMetaType<QVariant>("QVariant");
    qRegisterMetaType<QQmlScriptString>("QQmlScriptString");
    qRegisterMetaType<QJSValue>("QJSValue");
    qRegisterMetaType<QQmlComponent::Status>("QQmlComponent::Status");
    qRegisterMetaType<QList<QObject*> >("QList<QObject*>");
    qRegisterMetaType<QList<int> >("QList<int>");
    qRegisterMetaType<QQmlV8Handle>("QQmlV8Handle");

    v8engine()->setEngine(q);

    rootContext = new QQmlContext(q,true);

    if (QCoreApplication::instance()->thread() == q->thread() &&
        QQmlEngineDebugService::isDebuggingEnabled()) {
        isDebugging = true;
        QQmlEngineDebugService::instance()->addEngine(q);
        QV8DebugService::initialize(v8engine());
        QV8ProfilerService::initialize();
        QQmlProfilerService::initialize();
        QDebugMessageService::instance();
    }

    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (!dataLocation.isEmpty())
        offlineStoragePath = dataLocation.replace(QLatin1Char('/'), QDir::separator())
                           + QDir::separator() + QLatin1String("QML")
                           + QDir::separator() + QLatin1String("OfflineStorage");
}

QQuickWorkerScriptEngine *QQmlEnginePrivate::getWorkerScriptEngine()
{
    Q_Q(QQmlEngine);
    if (!workerScriptEngine)
        workerScriptEngine = new QQuickWorkerScriptEngine(q);
    return workerScriptEngine;
}

/*!
  \class QQmlEngine
  \since 5.0
  \inmodule QtQml
  \brief The QQmlEngine class provides an environment for instantiating QML components.
  \mainclass

  Each QML component is instantiated in a QQmlContext.
  QQmlContext's are essential for passing data to QML
  components.  In QML, contexts are arranged hierarchically and this
  hierarchy is managed by the QQmlEngine.

  Prior to creating any QML components, an application must have
  created a QQmlEngine to gain access to a QML context.  The
  following example shows how to create a simple Text item.

  \code
  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData("import QtQuick 2.0\nText { text: \"Hello world!\" }", QUrl());
  QQuickItem *item = qobject_cast<QQuickItem *>(component.create());

  //add item to view, etc
  ...
  \endcode

  In this case, the Text item will be created in the engine's
  \l {QQmlEngine::rootContext()}{root context}.

  Note that the QtQuick 1 version is called QDeclarativeEngine.

  \sa QQmlComponent, QQmlContext
*/

/*!
  Create a new QQmlEngine with the given \a parent.
*/
QQmlEngine::QQmlEngine(QObject *parent)
: QJSEngine(*new QQmlEnginePrivate(this), parent)
{
    Q_D(QQmlEngine);
    d->init();
}

/*!
  Destroys the QQmlEngine.

  Any QQmlContext's created on this engine will be
  invalidated, but not destroyed (unless they are parented to the
  QQmlEngine object).
*/
QQmlEngine::~QQmlEngine()
{
    Q_D(QQmlEngine);
    if (d->isDebugging) {
        QQmlEngineDebugService::instance()->remEngine(this);
    }

    // Emit onDestruction signals for the root context before
    // we destroy the contexts, engine, Module APIs etc. that
    // may be required to handle the destruction signal.
    QQmlContextData::get(rootContext())->emitDestruction();

    // if we are the parent of any of the qobject module api instances,
    // we need to remove them from our internal list, in order to prevent
    // a segfault in engine private dtor.
    QList<QQmlMetaType::ModuleApi> keys = d->moduleApiInstances.keys();
    QObject *currQObjectApi = 0;
    QQmlMetaType::ModuleApiInstance *currInstance = 0;
    foreach (const QQmlMetaType::ModuleApi &key, keys) {
        currInstance = d->moduleApiInstances.value(key);
        currQObjectApi = currInstance->qobjectApi;
        if (this->children().contains(currQObjectApi)) {
            delete currQObjectApi;
            delete currInstance;
            d->moduleApiInstances.remove(key);
        }
    }

    if (d->incubationController)
        d->incubationController->d = 0;
}

/*! \fn void QQmlEngine::quit()
    This signal is emitted when the QML loaded by the engine would like to quit.
 */

/*! \fn void QQmlEngine::warnings(const QList<QQmlError> &warnings)
    This signal is emitted when \a warnings messages are generated by QML.
 */

/*!
  Clears the engine's internal component cache.

  This function causes the property metadata of all components previously
  loaded by the engine to be destroyed.  All previously loaded components and
  the property bindings for all extant objects created from those components will
  cease to function.

  This function returns the engine to a state where it does not contain any loaded
  component data.  This may be useful in order to reload a smaller subset of the
  previous component set, or to load a new version of a previously loaded component.

  Once the component cache has been cleared, components must be loaded before
  any new objects can be created.
 */
void QQmlEngine::clearComponentCache()
{
    Q_D(QQmlEngine);
    d->typeLoader.clearCache();
}

/*!
  Returns the engine's root context.

  The root context is automatically created by the QQmlEngine.
  Data that should be available to all QML component instances
  instantiated by the engine should be put in the root context.

  Additional data that should only be available to a subset of
  component instances should be added to sub-contexts parented to the
  root context.
*/
QQmlContext *QQmlEngine::rootContext() const
{
    Q_D(const QQmlEngine);
    return d->rootContext;
}

/*!
  Sets the \a factory to use for creating QNetworkAccessManager(s).

  QNetworkAccessManager is used for all network access by QML.  By
  implementing a factory it is possible to create custom
  QNetworkAccessManager with specialized caching, proxy and cookie
  support.

  The factory must be set before executing the engine.
*/
void QQmlEngine::setNetworkAccessManagerFactory(QQmlNetworkAccessManagerFactory *factory)
{
    Q_D(QQmlEngine);
    QMutexLocker locker(&d->mutex);
    d->networkAccessManagerFactory = factory;
}

/*!
  Returns the current QQmlNetworkAccessManagerFactory.

  \sa setNetworkAccessManagerFactory()
*/
QQmlNetworkAccessManagerFactory *QQmlEngine::networkAccessManagerFactory() const
{
    Q_D(const QQmlEngine);
    return d->networkAccessManagerFactory;
}

void QQmlEnginePrivate::registerFinalizeCallback(QObject *obj, int index)
{
    if (activeVME) {
        activeVME->finalizeCallbacks.append(qMakePair(QQmlGuard<QObject>(obj), index));
    } else {
        void *args[] = { 0 };
        QMetaObject::metacall(obj, QMetaObject::InvokeMetaMethod, index, args);
    }
}

QNetworkAccessManager *QQmlEnginePrivate::createNetworkAccessManager(QObject *parent) const
{
    QMutexLocker locker(&mutex);
    QNetworkAccessManager *nam;
    if (networkAccessManagerFactory) {
        nam = networkAccessManagerFactory->create(parent);
    } else {
        nam = new QNetworkAccessManager(parent);
    }

    return nam;
}

QNetworkAccessManager *QQmlEnginePrivate::getNetworkAccessManager() const
{
    Q_Q(const QQmlEngine);
    if (!networkAccessManager)
        networkAccessManager = createNetworkAccessManager(const_cast<QQmlEngine*>(q));
    return networkAccessManager;
}

/*!
  Returns a common QNetworkAccessManager which can be used by any QML
  element instantiated by this engine.

  If a QQmlNetworkAccessManagerFactory has been set and a
  QNetworkAccessManager has not yet been created, the
  QQmlNetworkAccessManagerFactory will be used to create the
  QNetworkAccessManager; otherwise the returned QNetworkAccessManager
  will have no proxy or cache set.

  \sa setNetworkAccessManagerFactory()
*/
QNetworkAccessManager *QQmlEngine::networkAccessManager() const
{
    Q_D(const QQmlEngine);
    return d->getNetworkAccessManager();
}

/*!

  Sets the \a provider to use for images requested via the \e
  image: url scheme, with host \a providerId. The QQmlEngine
  takes ownership of \a provider.

  Image providers enable support for pixmap and threaded image
  requests. See the QQuickImageProvider documentation for details on
  implementing and using image providers.

  All required image providers should be added to the engine before any
  QML sources files are loaded.

  \sa removeImageProvider(), QQuickImageProvider, QQmlImageProviderBase
*/
void QQmlEngine::addImageProvider(const QString &providerId, QQmlImageProviderBase *provider)
{
    Q_D(QQmlEngine);
    QMutexLocker locker(&d->mutex);
    d->imageProviders.insert(providerId.toLower(), QSharedPointer<QQmlImageProviderBase>(provider));
}

/*!
  Returns the image provider set for \a providerId.

  Returns the provider if it was found; otherwise returns 0.

  \sa QQuickImageProvider
*/
QQmlImageProviderBase *QQmlEngine::imageProvider(const QString &providerId) const
{
    Q_D(const QQmlEngine);
    QMutexLocker locker(&d->mutex);
    return d->imageProviders.value(providerId).data();
}

/*!
  Removes the image provider for \a providerId.

  \sa addImageProvider(), QQuickImageProvider
*/
void QQmlEngine::removeImageProvider(const QString &providerId)
{
    Q_D(QQmlEngine);
    QMutexLocker locker(&d->mutex);
    d->imageProviders.take(providerId);
}

/*!
  Return the base URL for this engine.  The base URL is only used to
  resolve components when a relative URL is passed to the
  QQmlComponent constructor.

  If a base URL has not been explicitly set, this method returns the
  application's current working directory.

  \sa setBaseUrl()
*/
QUrl QQmlEngine::baseUrl() const
{
    Q_D(const QQmlEngine);
    if (d->baseUrl.isEmpty()) {
        return QUrl::fromLocalFile(QDir::currentPath() + QDir::separator());
    } else {
        return d->baseUrl;
    }
}

/*!
  Set the  base URL for this engine to \a url.

  \sa baseUrl()
*/
void QQmlEngine::setBaseUrl(const QUrl &url)
{
    Q_D(QQmlEngine);
    d->baseUrl = url;
}

/*!
  Returns true if warning messages will be output to stderr in addition
  to being emitted by the warnings() signal, otherwise false.

  The default value is true.
*/
bool QQmlEngine::outputWarningsToStandardError() const
{
    Q_D(const QQmlEngine);
    return d->outputWarningsToStdErr;
}

/*!
  Set whether warning messages will be output to stderr to \a enabled.

  If \a enabled is true, any warning messages generated by QML will be
  output to stderr and emitted by the warnings() signal.  If \a enabled
  is false, on the warnings() signal will be emitted.  This allows
  applications to handle warning output themselves.

  The default value is true.
*/
void QQmlEngine::setOutputWarningsToStandardError(bool enabled)
{
    Q_D(QQmlEngine);
    d->outputWarningsToStdErr = enabled;
}

/*!
  Attempt to free unused memory.
*/
void QQmlEngine::collectGarbage()
{
    QV8Engine::gc();
}

/*!
  Returns the QQmlContext for the \a object, or 0 if no
  context has been set.

  When the QQmlEngine instantiates a QObject, the context is
  set automatically.
  */
QQmlContext *QQmlEngine::contextForObject(const QObject *object)
{
    if(!object)
        return 0;

    QObjectPrivate *priv = QObjectPrivate::get(const_cast<QObject *>(object));

    QQmlData *data =
        static_cast<QQmlData *>(priv->declarativeData);

    if (!data)
        return 0;
    else if (data->outerContext)
        return data->outerContext->asQQmlContext();
    else
        return 0;
}

/*!
  Sets the QQmlContext for the \a object to \a context.
  If the \a object already has a context, a warning is
  output, but the context is not changed.

  When the QQmlEngine instantiates a QObject, the context is
  set automatically.
 */
void QQmlEngine::setContextForObject(QObject *object, QQmlContext *context)
{
    if (!object || !context)
        return;

    QQmlData *data = QQmlData::get(object, true);
    if (data->context) {
        qWarning("QQmlEngine::setContextForObject(): Object already has a QQmlContext");
        return;
    }

    QQmlContextData *contextData = QQmlContextData::get(context);
    contextData->addObject(object);
}

/*!
  \enum QQmlEngine::ObjectOwnership

  Ownership controls whether or not QML automatically destroys the
  QObject when the object is garbage collected by the JavaScript
  engine.  The two ownership options are:

  \value CppOwnership The object is owned by C++ code, and will
  never be deleted by QML.  The JavaScript destroy() method cannot be
  used on objects with CppOwnership.  This option is similar to
  QScriptEngine::QtOwnership.

  \value JavaScriptOwnership The object is owned by JavaScript.
  When the object is returned to QML as the return value of a method
  call or property access, QML will track it, and delete the object
  if there are no remaining JavaScript references to it and it has no
  QObject::parent().  An object tracked by one QQmlEngine
  will be deleted during that QQmlEngine's destructor, and thus
  JavaScript references between objects with JavaScriptOwnership from
  two different engines will not be valid after the deletion of one of
  those engines.  This option is similar to QScriptEngine::ScriptOwnership.

  Generally an application doesn't need to set an object's ownership
  explicitly.  QML uses a heuristic to set the default object
  ownership.  By default, an object that is created by QML has
  JavaScriptOwnership.  The exception to this are the root objects
  created by calling QQmlComponent::create() or
  QQmlComponent::beginCreate() which have CppOwnership by
  default.  The ownership of these root-level objects is considered to
  have been transferred to the C++ caller.

  Objects not-created by QML have CppOwnership by default.  The
  exception to this is objects returned from a C++ method call.  The
  ownership of these objects is passed to JavaScript.

  Calling setObjectOwnership() overrides the default ownership
  heuristic used by QML.
*/

/*!
  Sets the \a ownership of \a object.
*/
void QQmlEngine::setObjectOwnership(QObject *object, ObjectOwnership ownership)
{
    if (!object)
        return;

    QQmlData *ddata = QQmlData::get(object, true);
    if (!ddata)
        return;

    ddata->indestructible = (ownership == CppOwnership)?true:false;
    ddata->explicitIndestructibleSet = true;
}

/*!
  Returns the ownership of \a object.
*/
QQmlEngine::ObjectOwnership QQmlEngine::objectOwnership(QObject *object)
{
    if (!object)
        return CppOwnership;

    QQmlData *ddata = QQmlData::get(object, false);
    if (!ddata)
        return CppOwnership;
    else
        return ddata->indestructible?CppOwnership:JavaScriptOwnership;
}

bool QQmlEngine::event(QEvent *e)
{
    Q_D(QQmlEngine);
    if (e->type() == QEvent::User)
        d->doDeleteInEngineThread();

    return QJSEngine::event(e);
}

void QQmlEnginePrivate::doDeleteInEngineThread()
{
    QFieldList<Deletable, &Deletable::next> list;
    mutex.lock();
    list.copyAndClear(toDeleteInEngineThread);
    mutex.unlock();

    while (Deletable *d = list.takeFirst())
        delete d;
}

Q_AUTOTEST_EXPORT void qmlExecuteDeferred(QObject *object)
{
    QQmlData *data = QQmlData::get(object);

    if (data && data->deferredComponent) {
        QQmlObjectCreatingProfiler prof;
        if (prof.enabled) {
            QQmlType *type = QQmlMetaType::qmlType(object->metaObject());
            prof.setTypeName(type ? type->qmlTypeName()
                                  : QString::fromUtf8(object->metaObject()->className()));
            if (data->outerContext)
                prof.setLocation(data->outerContext->url, data->lineNumber, data->columnNumber);
        }
        QQmlEnginePrivate *ep = QQmlEnginePrivate::get(data->context->engine);

        QQmlComponentPrivate::ConstructionState state;
        QQmlComponentPrivate::beginDeferred(ep, object, &state);

        data->deferredComponent->release();
        data->deferredComponent = 0;

        QQmlComponentPrivate::complete(ep, &state);
    }
}

QQmlContext *qmlContext(const QObject *obj)
{
    return QQmlEngine::contextForObject(obj);
}

QQmlEngine *qmlEngine(const QObject *obj)
{
    QQmlData *data = QQmlData::get(obj, false);
    if (!data || !data->context)
        return 0;
    return data->context->engine;
}

QObject *qmlAttachedPropertiesObjectById(int id, const QObject *object, bool create)
{
    QQmlData *data = QQmlData::get(object);
    if (!data)
        return 0; // Attached properties are only on objects created by QML

    QObject *rv = data->hasExtendedData()?data->attachedProperties()->value(id):0;
    if (rv || !create)
        return rv;

    QQmlAttachedPropertiesFunc pf = QQmlMetaType::attachedPropertiesFuncById(id);
    if (!pf)
        return 0;

    rv = pf(const_cast<QObject *>(object));

    if (rv)
        data->attachedProperties()->insert(id, rv);

    return rv;
}

QObject *qmlAttachedPropertiesObject(int *idCache, const QObject *object,
                                     const QMetaObject *attachedMetaObject, bool create)
{
    if (*idCache == -1)
        *idCache = QQmlMetaType::attachedPropertiesFuncId(attachedMetaObject);

    if (*idCache == -1 || !object)
        return 0;

    return qmlAttachedPropertiesObjectById(*idCache, object, create);
}

QQmlDebuggingEnabler::QQmlDebuggingEnabler(bool printWarning)
{
#ifndef QQML_NO_DEBUG_PROTOCOL
    if (!QQmlEnginePrivate::qml_debugging_enabled
            && printWarning) {
        qDebug("QML debugging is enabled. Only use this in a safe environment.");
    }
    QQmlEnginePrivate::qml_debugging_enabled = true;
#endif
}


class QQmlDataExtended {
public:
    QQmlDataExtended();
    ~QQmlDataExtended();

    QHash<int, QObject *> attachedProperties;
};

QQmlDataExtended::QQmlDataExtended()
{
}

QQmlDataExtended::~QQmlDataExtended()
{
}

void QQmlData::NotifyList::layout(QQmlNotifierEndpoint *endpoint)
{
    if (endpoint->next)
        layout(endpoint->next);

    int index = endpoint->sourceSignal;
    index = qMin(index, 0xFFFF - 1);

    endpoint->next = notifies[index];
    if (endpoint->next) endpoint->next->prev = &endpoint->next;
    endpoint->prev = &notifies[index];
    notifies[index] = endpoint;
}

void QQmlData::NotifyList::layout()
{
    Q_ASSERT(maximumTodoIndex >= notifiesSize);

    if (todo) {
        QQmlNotifierEndpoint **old = notifies;
        const int reallocSize = (maximumTodoIndex + 1) * sizeof(QQmlNotifierEndpoint*);
        notifies = (QQmlNotifierEndpoint**)realloc(notifies, reallocSize);
        const int memsetSize = (maximumTodoIndex - notifiesSize + 1) *
                               sizeof(QQmlNotifierEndpoint*);
        memset(notifies + notifiesSize, 0, memsetSize);

        if (notifies != old) {
            for (int ii = 0; ii < notifiesSize; ++ii)
                if (notifies[ii])
                    notifies[ii]->prev = &notifies[ii];
        }

        notifiesSize = maximumTodoIndex + 1;

        layout(todo);
    }

    maximumTodoIndex = 0;
    todo = 0;
}

void QQmlData::addNotify(int index, QQmlNotifierEndpoint *endpoint)
{
    if (!notifyList) {
        notifyList = (NotifyList *)malloc(sizeof(NotifyList));
        notifyList->connectionMask = 0;
        notifyList->maximumTodoIndex = 0;
        notifyList->notifiesSize = 0;
        notifyList->todo = 0;
        notifyList->notifies = 0;
    }

    Q_ASSERT(!endpoint->isConnected());

    index = qMin(index, 0xFFFF - 1);
    notifyList->connectionMask |= (1ULL << quint64(index % 64));

    if (index < notifyList->notifiesSize) {

        endpoint->next = notifyList->notifies[index];
        if (endpoint->next) endpoint->next->prev = &endpoint->next;
        endpoint->prev = &notifyList->notifies[index];
        notifyList->notifies[index] = endpoint;

    } else {
        notifyList->maximumTodoIndex = qMax(int(notifyList->maximumTodoIndex), index);

        endpoint->next = notifyList->todo;
        if (endpoint->next) endpoint->next->prev = &endpoint->next;
        endpoint->prev = &notifyList->todo;
        notifyList->todo = endpoint;
    }
}

bool QQml_isSignalConnected(QObject *obj, int signal_index, int index)
{
    QQmlData *data = QQmlData::get(obj);
    return QObjectPrivate::get(obj)->isSignalConnected(signal_index) || (data && data->signalHasEndpoint(index));
}

/*
    index MUST be the index returned by QMetaMethod::index()
    This is different than the index returned by QObjectPrivate::signalIndex()
*/
bool QQmlData::signalHasEndpoint(int index)
{
    return notifyList && (notifyList->connectionMask & (1ULL << quint64(index % 64)));
}

QHash<int, QObject *> *QQmlData::attachedProperties() const
{
    if (!extendedData) extendedData = new QQmlDataExtended;
    return &extendedData->attachedProperties;
}

void QQmlData::destroyed(QObject *object)
{
    if (deferredComponent)
        deferredComponent->release();

    if (nextContextObject)
        nextContextObject->prevContextObject = prevContextObject;
    if (prevContextObject)
        *prevContextObject = nextContextObject;

    QQmlAbstractBinding *binding = bindings;
    while (binding) {
        QQmlAbstractBinding *next = binding->m_nextBinding;
        binding->m_prevBinding = 0;
        binding->m_nextBinding = 0;
        binding->destroy();
        binding = next;
    }

    QQmlAbstractBoundSignal *signalHandler = signalHandlers;
    while (signalHandler) {
        QQmlAbstractBoundSignal *next = signalHandler->m_nextSignal;
        signalHandler->m_prevSignal = 0;
        signalHandler->m_nextSignal = 0;
        delete signalHandler;
        signalHandler = next;
    }

    if (bindingBits)
        free(bindingBits);

    if (propertyCache)
        propertyCache->release();

    if (ownContext && context)
        context->destroy();

    while (guards) {
        QQmlGuard<QObject> *guard = static_cast<QQmlGuard<QObject> *>(guards);
        *guard = (QObject *)0;
        guard->objectDestroyed(object);
    }

    if (notifyList) {
        while (notifyList->todo)
            notifyList->todo->disconnect();
        for (int ii = 0; ii < notifyList->notifiesSize; ++ii) {
            while (QQmlNotifierEndpoint *ep = notifyList->notifies[ii])
                ep->disconnect();
        }
        free(notifyList->notifies);
        free(notifyList);
        notifyList = 0;
    }

    if (extendedData)
        delete extendedData;

    v8object.Clear(); // The WeakReference handler will clean the actual handle

    if (ownMemory)
        delete this;
}

void QQmlData::parentChanged(QObject *object, QObject *parent)
{
    Q_UNUSED(object);
    Q_UNUSED(parent);
}

bool QQmlData::hasBindingBit(int bit) const
{
    if (bindingBitsSize > bit)
        return bindingBits[bit / 32] & (1 << (bit % 32));
    else
        return false;
}

void QQmlData::clearBindingBit(int bit)
{
    if (bindingBitsSize > bit)
        bindingBits[bit / 32] &= ~(1 << (bit % 32));
}

void QQmlData::setBindingBit(QObject *obj, int bit)
{
    if (bindingBitsSize <= bit) {
        int props = obj->metaObject()->propertyCount();
        Q_ASSERT(bit < props);

        int arraySize = (props + 31) / 32;
        int oldArraySize = bindingBitsSize / 32;

        bindingBits = (quint32 *)realloc(bindingBits,
                                         arraySize * sizeof(quint32));

        memset(bindingBits + oldArraySize,
               0x00,
               sizeof(quint32) * (arraySize - oldArraySize));

        bindingBitsSize = arraySize * 32;
    }

    bindingBits[bit / 32] |= (1 << (bit % 32));
}

void QQmlEnginePrivate::sendQuit()
{
    Q_Q(QQmlEngine);
    emit q->quit();
    if (q->receivers(SIGNAL(quit())) == 0) {
        qWarning("Signal QQmlEngine::quit() emitted, but no receivers connected to handle it.");
    }
}

static void dumpwarning(const QQmlError &error)
{
    qWarning().nospace() << qPrintable(error.toString());
}

static void dumpwarning(const QList<QQmlError> &errors)
{
    for (int ii = 0; ii < errors.count(); ++ii)
        dumpwarning(errors.at(ii));
}

void QQmlEnginePrivate::warning(const QQmlError &error)
{
    Q_Q(QQmlEngine);
    q->warnings(QList<QQmlError>() << error);
    if (outputWarningsToStdErr)
        dumpwarning(error);
}

void QQmlEnginePrivate::warning(const QList<QQmlError> &errors)
{
    Q_Q(QQmlEngine);
    q->warnings(errors);
    if (outputWarningsToStdErr)
        dumpwarning(errors);
}

void QQmlEnginePrivate::warning(QQmlEngine *engine, const QQmlError &error)
{
    if (engine)
        QQmlEnginePrivate::get(engine)->warning(error);
    else
        dumpwarning(error);
}

void QQmlEnginePrivate::warning(QQmlEngine *engine, const QList<QQmlError> &error)
{
    if (engine)
        QQmlEnginePrivate::get(engine)->warning(error);
    else
        dumpwarning(error);
}

void QQmlEnginePrivate::warning(QQmlEnginePrivate *engine, const QQmlError &error)
{
    if (engine)
        engine->warning(error);
    else
        dumpwarning(error);
}

void QQmlEnginePrivate::warning(QQmlEnginePrivate *engine, const QList<QQmlError> &error)
{
    if (engine)
        engine->warning(error);
    else
        dumpwarning(error);
}

/*
   This function should be called prior to evaluation of any js expression,
   so that scarce resources are not freed prematurely (eg, if there is a
   nested javascript expression).
 */
void QQmlEnginePrivate::referenceScarceResources()
{
    scarceResourcesRefCount += 1;
}

/*
   This function should be called after evaluation of the js expression is
   complete, and so the scarce resources may be freed safely.
 */
void QQmlEnginePrivate::dereferenceScarceResources()
{
    Q_ASSERT(scarceResourcesRefCount > 0);
    scarceResourcesRefCount -= 1;

    // if the refcount is zero, then evaluation of the "top level"
    // expression must have completed.  We can safely release the
    // scarce resources.
    if (scarceResourcesRefCount == 0) {
        // iterate through the list and release them all.
        // note that the actual SRD is owned by the JS engine,
        // so we cannot delete the SRD; but we can free the
        // memory used by the variant in the SRD.
        while (ScarceResourceData *sr = scarceResources.first()) {
            sr->data = QVariant();
            scarceResources.remove(sr);
        }
    }
}

/*!
  Adds \a path as a directory where the engine searches for
  installed modules in a URL-based directory structure.
  The \a path may be a local filesystem directory or a URL.

  The newly added \a path will be first in the importPathList().

  \sa setImportPathList(), {QML Modules}
*/
void QQmlEngine::addImportPath(const QString& path)
{
    Q_D(QQmlEngine);
    d->importDatabase.addImportPath(path);
}

/*!
  Returns the list of directories where the engine searches for
  installed modules in a URL-based directory structure.

  For example, if \c /opt/MyApp/lib/imports is in the path, then QML that
  imports \c com.mycompany.Feature will cause the QQmlEngine to look
  in \c /opt/MyApp/lib/imports/com/mycompany/Feature/ for the components
  provided by that module. A \c qmldir file is required for defining the
  type version mapping and possibly QML extensions plugins.

  By default, the list contains the directory of the application executable,
  paths specified in the \c QML_IMPORT_PATH environment variable,
  and the builtin \c ImportsPath from QLibraryInfo.

  \sa addImportPath(), setImportPathList()
*/
QStringList QQmlEngine::importPathList() const
{
    Q_D(const QQmlEngine);
    return d->importDatabase.importPathList();
}

/*!
  Sets \a paths as the list of directories where the engine searches for
  installed modules in a URL-based directory structure.

  By default, the list contains the directory of the application executable,
  paths specified in the \c QML_IMPORT_PATH environment variable,
  and the builtin \c ImportsPath from QLibraryInfo.

  \sa importPathList(), addImportPath()
  */
void QQmlEngine::setImportPathList(const QStringList &paths)
{
    Q_D(QQmlEngine);
    d->importDatabase.setImportPathList(paths);
}


/*!
  Adds \a path as a directory where the engine searches for
  native plugins for imported modules (referenced in the \c qmldir file).

  By default, the list contains only \c .,  i.e. the engine searches
  in the directory of the \c qmldir file itself.

  The newly added \a path will be first in the pluginPathList().

  \sa setPluginPathList()
*/
void QQmlEngine::addPluginPath(const QString& path)
{
    Q_D(QQmlEngine);
    d->importDatabase.addPluginPath(path);
}


/*!
  Returns the list of directories where the engine searches for
  native plugins for imported modules (referenced in the \c qmldir file).

  By default, the list contains only \c .,  i.e. the engine searches
  in the directory of the \c qmldir file itself.

  \sa addPluginPath(), setPluginPathList()
*/
QStringList QQmlEngine::pluginPathList() const
{
    Q_D(const QQmlEngine);
    return d->importDatabase.pluginPathList();
}

/*!
  Sets the list of directories where the engine searches for
  native plugins for imported modules (referenced in the \c qmldir file)
  to \a paths.

  By default, the list contains only \c .,  i.e. the engine searches
  in the directory of the \c qmldir file itself.

  \sa pluginPathList(), addPluginPath()
  */
void QQmlEngine::setPluginPathList(const QStringList &paths)
{
    Q_D(QQmlEngine);
    d->importDatabase.setPluginPathList(paths);
}

/*!
  Imports the plugin named \a filePath with the \a uri provided.
  Returns true if the plugin was successfully imported; otherwise returns false.

  On failure and if non-null, the \a errors list will have any errors which occurred prepended to it.

  The plugin has to be a Qt plugin which implements the QQmlExtensionPlugin interface.
*/
bool QQmlEngine::importPlugin(const QString &filePath, const QString &uri, QList<QQmlError> *errors)
{
    Q_D(QQmlEngine);
    return d->importDatabase.importPlugin(filePath, uri, errors);
}

/*!
  Imports the plugin named \a filePath with the \a uri provided.
  Returns true if the plugin was successfully imported; otherwise returns false.

  On failure and if non-null, *\a errorString will be set to a message describing the failure.

  The plugin has to be a Qt plugin which implements the QQmlExtensionPlugin interface.
*/
bool QQmlEngine::importPlugin(const QString &filePath, const QString &uri, QString *errorString)
{
    Q_D(QQmlEngine);
    QList<QQmlError> errors;
    bool retn = d->importDatabase.importPlugin(filePath, uri, &errors);
    if (!errors.isEmpty()) {
        QString builtError;
        for (int i = 0; i < errors.size(); ++i) {
            builtError = QString(QLatin1String("%1\n        %2"))
                    .arg(builtError)
                    .arg(errors.at(i).toString());
        }
        *errorString = builtError;
    }
    return retn;
}

/*!
  \property QQmlEngine::offlineStoragePath
  \brief the directory for storing offline user data

  Returns the directory where SQL and other offline
  storage is placed.

  QQuickWebView and the SQL databases created with openDatabase()
  are stored here.

  The default is QML/OfflineStorage in the platform-standard
  user application data directory.

  Note that the path may not currently exist on the filesystem, so
  callers wanting to \e create new files at this location should create
  it first - see QDir::mkpath().
*/
void QQmlEngine::setOfflineStoragePath(const QString& dir)
{
    Q_D(QQmlEngine);
    d->offlineStoragePath = dir;
}

QString QQmlEngine::offlineStoragePath() const
{
    Q_D(const QQmlEngine);
    return d->offlineStoragePath;
}

static void voidptr_destructor(void *v)
{
    void **ptr = (void **)v;
    delete ptr;
}

static void *voidptr_constructor(const void *v)
{
    if (!v) {
        return new void*;
    } else {
        return new void*(*(void **)v);
    }
}

QQmlPropertyCache *QQmlEnginePrivate::createCache(const QMetaObject *mo)
{
    Q_Q(QQmlEngine);

    if (!mo->superClass()) {
        QQmlPropertyCache *rv = new QQmlPropertyCache(q, mo);
        propertyCache.insert(mo, rv);
        return rv;
    } else {
        QQmlPropertyCache *super = cache(mo->superClass());
        QQmlPropertyCache *rv = super->copyAndAppend(q, mo);
        propertyCache.insert(mo, rv);
        return rv;
    }
}

QQmlPropertyCache *QQmlEnginePrivate::createCache(QQmlType *type, int minorVersion,
                                                                  QQmlError &error)
{
    QList<QQmlType *> types;

    int maxMinorVersion = 0;

    const QMetaObject *metaObject = type->metaObject();

    while (metaObject) {
        QQmlType *t = QQmlMetaType::qmlType(metaObject, type->module(),
                                                            type->majorVersion(), minorVersion);
        if (t) {
            maxMinorVersion = qMax(maxMinorVersion, t->minorVersion());
            types << t;
        } else {
            types << 0;
        }

        metaObject = metaObject->superClass();
    }

    if (QQmlPropertyCache *c = typePropertyCache.value(qMakePair(type, maxMinorVersion))) {
        c->addref();
        typePropertyCache.insert(qMakePair(type, minorVersion), c);
        return c;
    }

    QQmlPropertyCache *raw = cache(type->metaObject());

    bool hasCopied = false;

    for (int ii = 0; ii < types.count(); ++ii) {
        QQmlType *currentType = types.at(ii);
        if (!currentType)
            continue;

        int rev = currentType->metaObjectRevision();
        int moIndex = types.count() - 1 - ii;

        if (raw->allowedRevisionCache[moIndex] != rev) {
            if (!hasCopied) {
                raw = raw->copy();
                hasCopied = true;
            }
            raw->allowedRevisionCache[moIndex] = rev;
        }
    }

    // Test revision compatibility - the basic rule is:
    //    * Anything that is excluded, cannot overload something that is not excluded *

    // Signals override:
    //    * other signals and methods of the same name.
    //    * properties named on<Signal Name>
    //    * automatic <property name>Changed notify signals

    // Methods override:
    //    * other methods of the same name

    // Properties override:
    //    * other elements of the same name

    bool overloadError = false;
    QString overloadName;

#if 0
    for (QQmlPropertyCache::StringCache::ConstIterator iter = raw->stringCache.begin();
         !overloadError && iter != raw->stringCache.end();
         ++iter) {

        QQmlPropertyData *d = *iter;
        if (raw->isAllowedInRevision(d))
            continue; // Not excluded - no problems

        // check that a regular "name" overload isn't happening
        QQmlPropertyData *current = d;
        while (!overloadError && current) {
            current = d->overrideData(current);
            if (current && raw->isAllowedInRevision(current))
                overloadError = true;
        }
    }
#endif

    if (overloadError) {
        if (hasCopied) raw->release();

        error.setDescription(QLatin1String("Type ") + type->qmlTypeName() + QLatin1String(" ") + QString::number(type->majorVersion()) + QLatin1String(".") + QString::number(minorVersion) + QLatin1String(" contains an illegal property \"") + overloadName + QLatin1String("\".  This is an error in the type's implementation."));
        return 0;
    }

    if (!hasCopied) raw->addref();
    typePropertyCache.insert(qMakePair(type, minorVersion), raw);

    if (minorVersion != maxMinorVersion) {
        raw->addref();
        typePropertyCache.insert(qMakePair(type, maxMinorVersion), raw);
    }

    return raw;
}

QQmlMetaType::ModuleApiInstance *
QQmlEnginePrivate::moduleApiInstance(const QQmlMetaType::ModuleApi &module)
{
    Locker locker(this);

    QQmlMetaType::ModuleApiInstance *a = moduleApiInstances.value(module);
    if (!a) {
        a = new QQmlMetaType::ModuleApiInstance;
        a->scriptCallback = module.script;
        a->qobjectCallback = module.qobject;
        a->instanceMetaObject = module.instanceMetaObject;
        moduleApiInstances.insert(module, a);
    }

    return a;
}

bool QQmlEnginePrivate::isQObject(int t)
{
    Locker locker(this);
    return m_compositeTypes.contains(t) || QQmlMetaType::isQObject(t);
}

QObject *QQmlEnginePrivate::toQObject(const QVariant &v, bool *ok) const
{
    Locker locker(this);
    int t = v.userType();
    if (t == QMetaType::QObjectStar || m_compositeTypes.contains(t)) {
        if (ok) *ok = true;
        return *(QObject **)(v.constData());
    } else {
        return QQmlMetaType::toQObject(v, ok);
    }
}

QQmlMetaType::TypeCategory QQmlEnginePrivate::typeCategory(int t) const
{
    Locker locker(this);
    if (m_compositeTypes.contains(t))
        return QQmlMetaType::Object;
    else if (m_qmlLists.contains(t))
        return QQmlMetaType::List;
    else
        return QQmlMetaType::typeCategory(t);
}

bool QQmlEnginePrivate::isList(int t) const
{
    Locker locker(this);
    return m_qmlLists.contains(t) || QQmlMetaType::isList(t);
}

int QQmlEnginePrivate::listType(int t) const
{
    Locker locker(this);
    QHash<int, int>::ConstIterator iter = m_qmlLists.find(t);
    if (iter != m_qmlLists.end())
        return *iter;
    else
        return QQmlMetaType::listType(t);
}

const QMetaObject *QQmlEnginePrivate::rawMetaObjectForType(int t) const
{
    Locker locker(this);
    QHash<int, QQmlCompiledData*>::ConstIterator iter = m_compositeTypes.find(t);
    if (iter != m_compositeTypes.end()) {
        return (*iter)->root;
    } else {
        QQmlType *type = QQmlMetaType::qmlType(t);
        return type?type->baseMetaObject():0;
    }
}

const QMetaObject *QQmlEnginePrivate::metaObjectForType(int t) const
{
    Locker locker(this);
    QHash<int, QQmlCompiledData*>::ConstIterator iter = m_compositeTypes.find(t);
    if (iter != m_compositeTypes.end()) {
        return (*iter)->root;
    } else {
        QQmlType *type = QQmlMetaType::qmlType(t);
        return type?type->metaObject():0;
    }
}

void QQmlEnginePrivate::registerCompositeType(QQmlCompiledData *data)
{
    QByteArray name = data->root->className();

    QByteArray ptr = name + '*';
    QByteArray lst = "QQmlListProperty<" + name + '>';

    int ptr_type = QMetaType::registerType(ptr.constData(), voidptr_destructor,
                                           voidptr_constructor);
    int lst_type = QMetaType::registerType(lst.constData(), voidptr_destructor,
                                           voidptr_constructor);

    data->addref();

    Locker locker(this);
    m_qmlLists.insert(lst_type, ptr_type);
    m_compositeTypes.insert(ptr_type, data);
}

bool QQml_isFileCaseCorrect(const QString &fileName)
{
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
    QFileInfo info(fileName);
    const QString absolute = info.absoluteFilePath();

#if defined(Q_OS_MAC)
    const QString canonical = info.canonicalFilePath();
#elif defined(Q_OS_WIN)
    wchar_t buffer[1024];

    DWORD rv = ::GetShortPathName((wchar_t*)absolute.utf16(), buffer, 1024);
    if (rv == 0 || rv >= 1024) return true;
    rv = ::GetLongPathName(buffer, buffer, 1024);
    if (rv == 0 || rv >= 1024) return true;

    const QString canonical = QString::fromWCharArray(buffer);
#endif

    const int absoluteLength = absolute.length();
    const int canonicalLength = canonical.length();

    const int length = qMin(absoluteLength, canonicalLength);
    for (int ii = 0; ii < length; ++ii) {
        const QChar &a = absolute.at(absoluteLength - 1 - ii);
        const QChar &c = canonical.at(canonicalLength - 1 - ii);

        if (a.toLower() != c.toLower())
            return true;
        if (a != c)
            return false;
    }
#else
    Q_UNUSED(fileName)
#endif
    return true;
}

/*!
    \fn QQmlEngine *qmlEngine(const QObject *object)
    \relates QQmlEngine

    Returns the QQmlEngine associated with \a object, if any.  This is equivalent to
    QQmlEngine::contextForObject(object)->engine(), but more efficient.
*/

/*!
    \fn QQmlContext *qmlContext(const QObject *object)
    \relates QQmlEngine

    Returns the QQmlContext associated with \a object, if any.  This is equivalent to
    QQmlEngine::contextForObject(object).
*/

QT_END_NAMESPACE
