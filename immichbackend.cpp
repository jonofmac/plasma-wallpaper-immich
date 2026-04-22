// SPDX-FileCopyrightText: 2026 Jonathan
// SPDX-License-Identifier: GPL-2.0-or-later

#include "immichbackend.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRandomGenerator>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString normalizeServerUrl(QString s)
{
    s = s.trimmed();
    if (s.isEmpty()) {
        return s;
    }
    if (!s.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        && !s.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        s = QLatin1String("https://") + s;
    }
    while (s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    return s;
}

QUrl apiUrl(const QString &apiBase, const QString &path)
{
    return QUrl(apiBase + path);
}

QString extensionForMime(const QString &mime)
{
    if (mime == QLatin1String("image/png")) {
        return QStringLiteral(".png");
    }
    if (mime == QLatin1String("image/webp")) {
        return QStringLiteral(".webp");
    }
    if (mime == QLatin1String("image/gif")) {
        return QStringLiteral(".gif");
    }
    if (mime == QLatin1String("image/heic") || mime == QLatin1String("image/heif")) {
        return QStringLiteral(".heic");
    }
    return QStringLiteral(".jpg");
}

} // namespace

ImmichBackend::ImmichBackend(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_slideTimer(new QTimer(this))
    , m_refreshDebounce(new QTimer(this))
{
    m_slideTimer->setTimerType(Qt::CoarseTimer);
    connect(m_slideTimer, &QTimer::timeout, this, &ImmichBackend::advanceSlide);

    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(400);
    connect(m_refreshDebounce, &QTimer::timeout, this, &ImmichBackend::performRefresh);

    m_networkRetryTimer = new QTimer(this);
    m_networkRetryTimer->setSingleShot(true);
    connect(m_networkRetryTimer, &QTimer::timeout, this, &ImmichBackend::performRefresh);

    if (QNetworkInformation::loadDefaultBackend()) {
        if (QNetworkInformation *const info = QNetworkInformation::instance()) {
            connect(info, &QNetworkInformation::reachabilityChanged, this, [this](QNetworkInformation::Reachability r) {
                if (r != QNetworkInformation::Reachability::Online || !m_wantsNetworkRetry) {
                    return;
                }
                m_networkRetryTimer->stop();
                m_refreshDebounce->stop();
                m_networkFailureStep = 0;
                QTimer::singleShot(2000, this, [this]() {
                    if (QNetworkInformation::instance()
                        && QNetworkInformation::instance()->reachability() == QNetworkInformation::Reachability::Online) {
                        performRefresh();
                    }
                });
            });
        }
    }

    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/plasma-wallpaper-immich");
    m_cacheDir = base;
    QDir().mkpath(m_cacheDir);
}

ImmichBackend::~ImmichBackend()
{
    abortPending();
}

void ImmichBackend::componentComplete()
{
    restoreLastWallpaperIfCached();
    m_refreshDebounce->stop();
    performRefresh();
}

QString ImmichBackend::serverUrl() const
{
    return m_serverUrl;
}

void ImmichBackend::setServerUrl(const QString &url)
{
    if (m_serverUrl == url) {
        return;
    }
    m_serverUrl = url;
    updateApiBase();
    Q_EMIT serverUrlChanged();
    scheduleRefresh();
}

QString ImmichBackend::email() const
{
    return m_email;
}

void ImmichBackend::setEmail(const QString &email)
{
    if (m_email == email) {
        return;
    }
    m_email = email;
    Q_EMIT emailChanged();
    scheduleRefresh();
}

QString ImmichBackend::password() const
{
    return m_password;
}

void ImmichBackend::setPassword(const QString &password)
{
    if (m_password == password) {
        return;
    }
    m_password = password;
    Q_EMIT passwordChanged();
    scheduleRefresh();
}

int ImmichBackend::authMode() const
{
    return m_authMode;
}

void ImmichBackend::setAuthMode(int mode)
{
    mode = qBound(0, mode, 1);
    if (m_authMode == mode) {
        return;
    }
    m_authMode = mode;
    Q_EMIT authModeChanged();
    scheduleRefresh();
}

QString ImmichBackend::apiKey() const
{
    return m_apiKey;
}

void ImmichBackend::setApiKey(const QString &key)
{
    if (m_apiKey == key) {
        return;
    }
    m_apiKey = key;
    Q_EMIT apiKeyChanged();
    scheduleRefresh();
}

int ImmichBackend::showMode() const
{
    return m_showMode;
}

void ImmichBackend::setShowMode(int mode)
{
    mode = qBound(0, mode, 3);
    if (m_showMode == mode) {
        return;
    }
    m_showMode = mode;
    Q_EMIT showModeChanged();
    scheduleRefresh();
}

QStringList ImmichBackend::selectedAlbumIds() const
{
    return m_selectedAlbumIds;
}

void ImmichBackend::setSelectedAlbumIds(const QStringList &ids)
{
    if (m_selectedAlbumIds == ids) {
        return;
    }
    m_selectedAlbumIds = ids;
    Q_EMIT selectedAlbumIdsChanged();
    scheduleRefresh();
}

QStringList ImmichBackend::selectedTagIds() const
{
    return m_selectedTagIds;
}

void ImmichBackend::setSelectedTagIds(const QStringList &ids)
{
    if (m_selectedTagIds == ids) {
        return;
    }
    m_selectedTagIds = ids;
    Q_EMIT selectedTagIdsChanged();
    scheduleRefresh();
}

int ImmichBackend::slideIntervalSeconds() const
{
    return m_slideIntervalSeconds;
}

void ImmichBackend::setSlideIntervalSeconds(int seconds)
{
    seconds = qMax(30, seconds);
    if (m_slideIntervalSeconds == seconds) {
        return;
    }
    m_slideIntervalSeconds = seconds;
    m_slideTimer->setInterval(seconds * 1000);
    Q_EMIT slideIntervalSecondsChanged();
}

bool ImmichBackend::loading() const
{
    return m_loading;
}

bool ImmichBackend::authenticated() const
{
    return m_authenticated;
}

bool ImmichBackend::canNavigate() const
{
    return m_assetIds.size() > 1;
}

int ImmichBackend::matchCount() const
{
    return m_matchCount;
}

QString ImmichBackend::localUrl() const
{
    return m_localUrl;
}

QString ImmichBackend::errorMessage() const
{
    return m_errorMessage;
}

QVariantList ImmichBackend::albums() const
{
    return m_albums;
}

QVariantList ImmichBackend::tags() const
{
    return m_tags;
}

void ImmichBackend::refreshAlbums()
{
    m_networkFailureStep = 0;
    m_wantsNetworkRetry = false;
    m_networkRetryTimer->stop();
    m_refreshDebounce->stop();
    performRefresh();
}

void ImmichBackend::nextImage()
{
    if (m_assetIds.size() <= 1) {
        return;
    }
    m_currentAssetIndex = (m_currentAssetIndex + 1) % m_assetIds.size();
    m_imageDownloadRetries = 0;
    m_slideTimer->stop();
    abortPending();
    m_phase = Phase::Idle;
    startDownloadCurrentAsset();
}

void ImmichBackend::previousImage()
{
    if (m_assetIds.size() <= 1) {
        return;
    }
    m_currentAssetIndex = (m_currentAssetIndex - 1 + m_assetIds.size()) % m_assetIds.size();
    m_imageDownloadRetries = 0;
    m_slideTimer->stop();
    abortPending();
    m_phase = Phase::Idle;
    startDownloadCurrentAsset();
}

void ImmichBackend::updateApiBase()
{
    QString n = normalizeServerUrl(m_serverUrl);
    if (n.endsWith(QStringLiteral("/api"), Qt::CaseInsensitive)) {
        m_apiBase = n;
    } else {
        m_apiBase = n.isEmpty() ? QString() : (n + QStringLiteral("/api"));
    }
}

void ImmichBackend::applyAuthHeader(QNetworkRequest &req) const
{
    if (m_authMode == 0) {
        req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    } else {
        req.setRawHeader("Authorization", (QStringLiteral("Bearer ") + m_accessToken).toUtf8());
    }
}

void ImmichBackend::abortPending()
{
    QPointer<QNetworkReply> guard(m_reply);
    if (!guard) {
        m_phase = Phase::Idle;
        return;
    }
    m_reply = nullptr;
    m_phase = Phase::Idle;
    // abort() can emit finished() synchronously; our handlers used to clear m_reply and
    // deleteLater the reply, which left a second deleteLater(nullptr) here. We disconnect
    // first. QPointer covers the case where abort() destroys the reply synchronously.
    guard->disconnect(this);
    guard->abort();
    if (guard) {
        guard->deleteLater();
    }
}

void ImmichBackend::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void ImmichBackend::setError(const QString &message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    Q_EMIT errorMessageChanged();
}

void ImmichBackend::setAuthenticated(bool authenticated)
{
    if (m_authenticated == authenticated) {
        return;
    }
    m_authenticated = authenticated;
    Q_EMIT authenticatedChanged();
}

void ImmichBackend::setMatchCount(int count)
{
    if (m_matchCount == count) {
        return;
    }
    m_matchCount = count;
    Q_EMIT matchCountChanged();
}

void ImmichBackend::setLocalUrl(const QString &pathOrUrl)
{
    if (m_localUrl == pathOrUrl) {
        return;
    }
    m_localUrl = pathOrUrl;

    const QString marker = m_cacheDir + QStringLiteral("/last_wallpaper.url");
    if (m_localUrl.isEmpty()) {
        QFile::remove(marker);
    } else {
        const QUrl u(m_localUrl);
        if (u.isLocalFile()) {
            QFile f(marker);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                f.write(m_localUrl.toUtf8());
            }
        }
    }

    Q_EMIT localUrlChanged();
}

void ImmichBackend::restoreLastWallpaperIfCached()
{
    const QString marker = m_cacheDir + QStringLiteral("/last_wallpaper.url");
    QFile f(marker);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    const QString url = QString::fromUtf8(f.readAll()).trimmed();
    if (url.isEmpty()) {
        return;
    }
    const QUrl u(url);
    if (!u.isLocalFile()) {
        return;
    }
    const QString path = u.toLocalFile();
    if (!QFile::exists(path)) {
        QFile::remove(marker);
        return;
    }
    setLocalUrl(url);
}

void ImmichBackend::scheduleRefresh()
{
    m_networkFailureStep = 0;
    m_wantsNetworkRetry = false;
    m_networkRetryTimer->stop();
    m_refreshDebounce->start();
}

void ImmichBackend::performRefresh()
{
    m_slideActiveBeforeRefresh = m_slideTimer->isActive();
    m_slideTimer->stop();
    m_slideTimer->setInterval(qMax(30, m_slideIntervalSeconds) * 1000);

    const QString n = normalizeServerUrl(m_serverUrl);
    const bool missingCreds = (m_authMode == 0) ? m_apiKey.trimmed().isEmpty() : (m_email.isEmpty() || m_password.isEmpty());
    if (n.isEmpty() || missingCreds) {
        setAuthenticated(false);
        setMatchCount(0);
        setError(QString());
        setLoading(false);
        m_albums.clear();
        Q_EMIT albumsChanged();
        m_tags.clear();
        Q_EMIT tagsChanged();
        setLocalUrl(QString());
        m_slideTimer->stop();
        m_refreshBackupAssetIds.clear();
        m_networkFailureStep = 0;
        m_wantsNetworkRetry = false;
        m_networkRetryTimer->stop();
        return;
    }

    updateApiBase();
    setAuthenticated(false);
    setMatchCount(0);
    setError(QString());
    setLoading(true);
    m_imageDownloadRetries = 0;
    abortPending();
    m_refreshBackupAssetIds = m_assetIds;
    m_refreshBackupIndex = m_currentAssetIndex;
    m_assetIds.clear();
    Q_EMIT canNavigateChanged();
    m_searchFilterQueue.clear();
    m_activeSearchFilterId.clear();
    m_searchPage = 1;
    m_currentAssetIndex = 0;

    if (m_authMode == 0) {
        setAuthenticated(true);
        m_accessToken.clear();
        startFetchAlbumList();
    } else {
        const QUrl url = apiUrl(m_apiBase, QStringLiteral("/auth/login"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Accept", "application/json");

        QJsonObject body;
        body[QStringLiteral("email")] = m_email;
        body[QStringLiteral("password")] = m_password;

        m_phase = Phase::Login;
        m_reply = m_nam->post(req, QJsonDocument(body).toJson());
        connect(m_reply, &QNetworkReply::finished, this, &ImmichBackend::handleLoginFinished);
    }
}

void ImmichBackend::handleLoginFinished()
{
    if (!m_reply || m_phase != Phase::Login) {
        return;
    }

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_phase = Phase::Idle;

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setAuthenticated(false);
        failRefreshNetworkError(QStringLiteral("Login failed: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject root = doc.object();
    m_accessToken = root[QStringLiteral("accessToken")].toString();
    if (m_accessToken.isEmpty()) {
        setAuthenticated(false);
        failRefreshNetworkError(QStringLiteral("Login failed: no access token in response"));
        return;
    }
    setAuthenticated(true);

    startFetchAlbumList();
}

void ImmichBackend::startFetchAlbumList()
{
    const QUrl url = apiUrl(m_apiBase, QStringLiteral("/albums"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    applyAuthHeader(req);

    m_phase = Phase::AlbumList;
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &ImmichBackend::handleAlbumListFinished);
}

void ImmichBackend::handleAlbumListFinished()
{
    if (!m_reply || m_phase != Phase::AlbumList) {
        return;
    }

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_phase = Phase::Idle;

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        failRefreshNetworkError(QStringLiteral("Could not list albums: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray arr = doc.array();
    if (arr.isEmpty() && doc.isObject()) {
        arr = doc.object()[QStringLiteral("albums")].toArray();
    }

    m_albums.clear();
    for (const auto &v : std::as_const(arr)) {
        const QJsonObject o = v.toObject();
        QVariantMap row;
        const QString id = o[QStringLiteral("id")].toString();
        QString name = o[QStringLiteral("albumName")].toString();
        if (name.isEmpty()) {
            name = o[QStringLiteral("name")].toString();
        }
        row[QStringLiteral("id")] = id;
        row[QStringLiteral("name")] = name.isEmpty() ? id : name;
        m_albums.append(row);
    }
    Q_EMIT albumsChanged();
    startFetchTagList();
}

void ImmichBackend::startFetchTagList()
{
    const QUrl url = apiUrl(m_apiBase, QStringLiteral("/tags"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    applyAuthHeader(req);

    m_phase = Phase::TagList;
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &ImmichBackend::handleTagListFinished);
}

void ImmichBackend::handleTagListFinished()
{
    if (!m_reply || m_phase != Phase::TagList) {
        return;
    }

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_phase = Phase::Idle;

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        failRefreshNetworkError(QStringLiteral("Could not list tags: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray arr = doc.array();
    if (arr.isEmpty() && doc.isObject()) {
        arr = doc.object()[QStringLiteral("tags")].toArray();
    }

    m_tags.clear();
    for (const auto &v : std::as_const(arr)) {
        const QJsonObject o = v.toObject();
        QVariantMap row;
        const QString id = o[QStringLiteral("id")].toString();
        QString name = o[QStringLiteral("value")].toString();
        if (name.isEmpty()) {
            name = o[QStringLiteral("name")].toString();
        }
        row[QStringLiteral("id")] = id;
        row[QStringLiteral("name")] = name.isEmpty() ? id : name;
        m_tags.append(row);
    }
    Q_EMIT tagsChanged();

    if (m_showMode == 0 && m_selectedAlbumIds.isEmpty()) {
        setMatchCount(0);
        setLoading(false);
        setError(QStringLiteral("Select at least one album in the wallpaper settings."));
        setLocalUrl(QString());
        m_slideTimer->stop();
        return;
    }

    if (m_showMode == 1 && m_selectedTagIds.isEmpty()) {
        setMatchCount(0);
        setLoading(false);
        setError(QStringLiteral("Select at least one tag in the wallpaper settings."));
        setLocalUrl(QString());
        m_slideTimer->stop();
        return;
    }

    m_assetIds.clear();
    Q_EMIT canNavigateChanged();
    setMatchCount(0);
    m_searchFilterQueue.clear();
    m_activeSearchFilterId.clear();

    if (m_showMode == 0 && m_selectedAlbumIds.size() > 1) {
        m_searchFilterQueue = m_selectedAlbumIds;
        startNextSearchBatch();
        return;
    }

    if (m_showMode == 1 && m_selectedTagIds.size() > 1) {
        m_searchFilterQueue = m_selectedTagIds;
        startNextSearchBatch();
        return;
    }

    m_searchPage = 1;
    startSearchAssetsPage(m_searchPage);
}

void ImmichBackend::startNextSearchBatch()
{
    if (m_searchFilterQueue.isEmpty()) {
        finishAssetCollection();
        return;
    }

    m_activeSearchFilterId = m_searchFilterQueue.takeFirst();
    m_searchPage = 1;
    startSearchAssetsPage(m_searchPage);
}

void ImmichBackend::startSearchAssetsPage(int page)
{
    const QUrl url = apiUrl(m_apiBase, QStringLiteral("/search/metadata"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    applyAuthHeader(req);

    QJsonObject body;
    body[QStringLiteral("page")] = page;
    body[QStringLiteral("size")] = 500;
    body[QStringLiteral("type")] = QStringLiteral("IMAGE");

    if (m_showMode == 0) {
        QJsonArray albumIds;
        if (!m_activeSearchFilterId.isEmpty()) {
            albumIds.append(m_activeSearchFilterId);
        } else {
            for (const QString &id : m_selectedAlbumIds) {
                albumIds.append(id);
            }
        }
        body[QStringLiteral("albumIds")] = albumIds;
    } else if (m_showMode == 1) {
        QJsonArray tagIds;
        QJsonArray tagNames;
        if (!m_activeSearchFilterId.isEmpty()) {
            tagIds.append(m_activeSearchFilterId);
            for (const QVariant &t : std::as_const(m_tags)) {
                const QVariantMap row = t.toMap();
                if (row.value(QStringLiteral("id")).toString() == m_activeSearchFilterId) {
                    const QString name = row.value(QStringLiteral("name")).toString();
                    if (!name.isEmpty()) {
                        tagNames.append(name);
                    }
                    break;
                }
            }
        } else {
            for (const QString &id : m_selectedTagIds) {
                tagIds.append(id);
                for (const QVariant &t : std::as_const(m_tags)) {
                    const QVariantMap row = t.toMap();
                    if (row.value(QStringLiteral("id")).toString() == id) {
                        const QString name = row.value(QStringLiteral("name")).toString();
                        if (!name.isEmpty()) {
                            tagNames.append(name);
                        }
                        break;
                    }
                }
            }
        }
        // tagIds is used by newer Immich versions; tags (names) is for compatibility with older API shapes.
        body[QStringLiteral("tagIds")] = tagIds;
        if (!tagNames.isEmpty()) {
            body[QStringLiteral("tags")] = tagNames;
        }
    } else if (m_showMode == 2) {
        body[QStringLiteral("isFavorite")] = true;
    }

    m_phase = Phase::SearchAssets;
    m_reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this, &ImmichBackend::handleSearchAssetsFinished);
}

void ImmichBackend::handleSearchAssetsFinished()
{
    if (!m_reply || m_phase != Phase::SearchAssets) {
        return;
    }

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_phase = Phase::Idle;

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        failRefreshNetworkError(QStringLiteral("Could not search assets: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonObject root = doc.object();
    const QJsonValue assetsValue = root.value(QStringLiteral("assets"));

    QJsonArray items;
    bool hasNext = false;

    if (assetsValue.isArray()) {
        items = assetsValue.toArray();
    } else if (assetsValue.isObject()) {
        const QJsonObject assetsObj = assetsValue.toObject();
        if (assetsObj.value(QStringLiteral("items")).isArray()) {
            items = assetsObj.value(QStringLiteral("items")).toArray();
        } else if (assetsObj.value(QStringLiteral("assets")).isArray()) {
            items = assetsObj.value(QStringLiteral("assets")).toArray();
        }
        hasNext = assetsObj.value(QStringLiteral("hasNextPage")).toBool(false)
            || assetsObj.value(QStringLiteral("nextPage")).toBool(false);

        const int totalPages = assetsObj.value(QStringLiteral("totalPages")).toInt(0);
        const int currentPage = assetsObj.value(QStringLiteral("page")).toInt(m_searchPage);
        if (totalPages > 0 && currentPage < totalPages) {
            hasNext = true;
        }
    }

    QSet<QString> seen;
    for (const QString &id : std::as_const(m_assetIds)) {
        seen.insert(id);
    }
    for (const auto &v : std::as_const(items)) {
        const QJsonObject o = v.toObject();
        const QString type = o.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("VIDEO")) {
            continue;
        }
        const QString id = o.value(QStringLiteral("id")).toString();
        if (!id.isEmpty() && !seen.contains(id)) {
            seen.insert(id);
            m_assetIds.append(id);
        }
    }

    if (hasNext) {
        m_searchPage += 1;
        startSearchAssetsPage(m_searchPage);
        return;
    }

    if (!m_searchFilterQueue.isEmpty()) {
        startNextSearchBatch();
        return;
    }

    m_activeSearchFilterId.clear();
    finishAssetCollection();
}

void ImmichBackend::finishAssetCollection()
{
    if (m_assetIds.isEmpty()) {
        setMatchCount(0);
        setLoading(false);
        if (m_showMode == 0) {
            setError(QStringLiteral("No photos found in the selected albums."));
        } else if (m_showMode == 1) {
            setError(QStringLiteral("No photos found for the selected tags."));
        } else if (m_showMode == 2) {
            setError(QStringLiteral("No favorite photos found."));
        } else {
            setError(QStringLiteral("No photos found."));
        }
        // Keep showing the last cached image (e.g. after resume when the network is not ready yet).
        m_slideTimer->stop();
        return;
    }

    auto *rng = QRandomGenerator::global();
    for (int i = m_assetIds.size() - 1; i > 0; --i) {
        const int j = rng->bounded(i + 1);
        m_assetIds.swapItemsAt(i, j);
    }

    m_currentAssetIndex = 0;
    setMatchCount(m_assetIds.size());
    Q_EMIT canNavigateChanged();
    startDownloadCurrentAsset();
}

void ImmichBackend::startDownloadCurrentAsset()
{
    if (m_assetIds.isEmpty()) {
        setLoading(false);
        return;
    }

    setLoading(true);

    const QString assetId = m_assetIds.at(m_currentAssetIndex);
    const QUrl url = apiUrl(m_apiBase, QStringLiteral("/assets/") + assetId + QStringLiteral("/original"));
    QNetworkRequest req(url);
    applyAuthHeader(req);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_phase = Phase::ImageDownload;
    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &ImmichBackend::handleImageDownloadFinished);
}

void ImmichBackend::handleImageDownloadFinished()
{
    if (!m_reply || m_phase != Phase::ImageDownload) {
        return;
    }

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_phase = Phase::Idle;

    const QByteArray data = reply->readAll();
    const QString assetId = m_assetIds.value(m_currentAssetIndex);

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        reply->deleteLater();
        if (m_imageDownloadRetries < 3) {
            ++m_imageDownloadRetries;
            setError(QStringLiteral("Image download failed, retrying… (%1/3)").arg(m_imageDownloadRetries));
            QTimer::singleShot(4000, this, [this]() { startDownloadCurrentAsset(); });
            return;
        }
        m_imageDownloadRetries = 0;
        setLoading(false);
        setError(QStringLiteral("Could not download image: %1").arg(err));
        m_wantsNetworkRetry = true;
        scheduleNetworkRetry();
        return;
    }

    const QString mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    reply->deleteLater();

    const QString ext = extensionForMime(mime);
    const QString imagePath = m_cacheDir + QLatin1Char('/') + assetId + ext;
    QFile f(imagePath);
    if (!f.open(QIODevice::WriteOnly)) {
        setLoading(false);
        setError(QStringLiteral("Could not write cache file."));
        return;
    }
    f.write(data);
    f.close();

    const QString fileUrl = QUrl::fromLocalFile(imagePath).toString();
    m_imageDownloadRetries = 0;
    m_networkFailureStep = 0;
    m_wantsNetworkRetry = false;
    m_networkRetryTimer->stop();
    setLocalUrl(fileUrl);
    setLoading(false);
    setError(QString());

    if (!m_slideTimer->isActive() && m_slideIntervalSeconds > 0) {
        m_slideTimer->start();
    }
}

void ImmichBackend::advanceSlide()
{
    if (m_assetIds.size() <= 1) {
        return;
    }
    m_currentAssetIndex = (m_currentAssetIndex + 1) % m_assetIds.size();
    m_imageDownloadRetries = 0;
    setLoading(true);
    abortPending();
    m_phase = Phase::Idle;
    startDownloadCurrentAsset();
}

void ImmichBackend::failRefreshNetworkError(const QString &message)
{
    setLoading(false);
    setError(message);
    m_phase = Phase::Idle;
    m_wantsNetworkRetry = true;

    if (m_assetIds.isEmpty() && !m_refreshBackupAssetIds.isEmpty()) {
        m_assetIds = m_refreshBackupAssetIds;
        m_currentAssetIndex = m_assetIds.isEmpty() ? 0 : qBound(0, m_refreshBackupIndex, m_assetIds.size() - 1);
        setMatchCount(m_assetIds.size());
        Q_EMIT canNavigateChanged();
        if (m_slideActiveBeforeRefresh && !m_assetIds.isEmpty() && m_slideIntervalSeconds > 0) {
            m_slideTimer->start();
        }
    }

    scheduleNetworkRetry();
}

void ImmichBackend::scheduleNetworkRetry()
{
    if (m_networkFailureStep >= 12) {
        return;
    }
    ++m_networkFailureStep;
    const int step = m_networkFailureStep;
    const int delayMs = qMin(300000, 5000 * (1 << qMin(step - 1, 6)));
    m_networkRetryTimer->stop();
    m_networkRetryTimer->setInterval(delayMs);
    m_networkRetryTimer->start();
}
