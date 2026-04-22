// SPDX-FileCopyrightText: 2026 Jonathan
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QQmlParserStatus>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class ImmichBackend : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString email READ email WRITE setEmail NOTIFY emailChanged)
    Q_PROPERTY(QString password READ password WRITE setPassword NOTIFY passwordChanged)
    Q_PROPERTY(int authMode READ authMode WRITE setAuthMode NOTIFY authModeChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(int showMode READ showMode WRITE setShowMode NOTIFY showModeChanged)
    Q_PROPERTY(QStringList selectedAlbumIds READ selectedAlbumIds WRITE setSelectedAlbumIds NOTIFY selectedAlbumIdsChanged)
    Q_PROPERTY(QStringList selectedTagIds READ selectedTagIds WRITE setSelectedTagIds NOTIFY selectedTagIdsChanged)
    Q_PROPERTY(int slideIntervalSeconds READ slideIntervalSeconds WRITE setSlideIntervalSeconds NOTIFY slideIntervalSecondsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool canNavigate READ canNavigate NOTIFY canNavigateChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY matchCountChanged)
    Q_PROPERTY(QString localUrl READ localUrl NOTIFY localUrlChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QVariantList albums READ albums NOTIFY albumsChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY tagsChanged)

public:
    explicit ImmichBackend(QObject *parent = nullptr);
    ~ImmichBackend() override;

    void classBegin() override {}
    void componentComplete() override;

    QString serverUrl() const;
    void setServerUrl(const QString &url);

    QString email() const;
    void setEmail(const QString &email);

    QString password() const;
    void setPassword(const QString &password);

    int authMode() const;
    void setAuthMode(int mode);

    QString apiKey() const;
    void setApiKey(const QString &key);

    int showMode() const;
    void setShowMode(int mode);

    QStringList selectedAlbumIds() const;
    void setSelectedAlbumIds(const QStringList &ids);

    QStringList selectedTagIds() const;
    void setSelectedTagIds(const QStringList &ids);

    int slideIntervalSeconds() const;
    void setSlideIntervalSeconds(int seconds);

    bool loading() const;
    bool authenticated() const;
    bool canNavigate() const;
    int matchCount() const;
    QString localUrl() const;
    QString errorMessage() const;
    QVariantList albums() const;
    QVariantList tags() const;

    Q_INVOKABLE void refreshAlbums();
    Q_INVOKABLE void nextImage();
    Q_INVOKABLE void previousImage();

signals:
    void serverUrlChanged();
    void emailChanged();
    void passwordChanged();
    void authModeChanged();
    void apiKeyChanged();
    void showModeChanged();
    void selectedAlbumIdsChanged();
    void selectedTagIdsChanged();
    void slideIntervalSecondsChanged();
    void loadingChanged();
    void authenticatedChanged();
    void canNavigateChanged();
    void matchCountChanged();
    void localUrlChanged();
    void errorMessageChanged();
    void albumsChanged();
    void tagsChanged();

private:
    void scheduleRefresh();
    void performRefresh();
    void abortPending();
    void setLoading(bool loading);
    void setAuthenticated(bool authenticated);
    void setMatchCount(int count);
    void setError(const QString &message);
    void setLocalUrl(const QString &pathOrUrl);
    void restoreLastWallpaperIfCached();
    void updateApiBase();
    void applyAuthHeader(QNetworkRequest &req) const;

    void handleLoginFinished();
    void startFetchAlbumList();
    void handleAlbumListFinished();
    void startFetchTagList();
    void handleTagListFinished();
    void startNextSearchBatch();
    void startSearchAssetsPage(int page);
    void handleSearchAssetsFinished();
    void finishAssetCollection();
    void startDownloadCurrentAsset();
    void handleImageDownloadFinished();

    void advanceSlide();
    void failRefreshNetworkError(const QString &message);
    void scheduleNetworkRetry();

    QString m_serverUrl;
    QString m_email;
    QString m_password;
    int m_authMode = 0; // 0=API key, 1=email/password
    QString m_apiKey;
    int m_showMode = 0;
    QStringList m_selectedAlbumIds;
    QStringList m_selectedTagIds;
    int m_slideIntervalSeconds = 900;

    bool m_loading = false;
    bool m_authenticated = false;
    int m_matchCount = 0;
    QString m_localUrl;
    QString m_errorMessage;
    QVariantList m_albums;
    QVariantList m_tags;

    QString m_accessToken;
    QString m_apiBase;

    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_reply = nullptr;

    enum class Phase { Idle, Login, AlbumList, TagList, SearchAssets, ImageDownload };
    Phase m_phase = Phase::Idle;

    QStringList m_assetIds;
    QStringList m_searchFilterQueue;
    QString m_activeSearchFilterId;
    int m_searchPage = 1;
    int m_currentAssetIndex = 0;

    QTimer *m_slideTimer = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    QTimer *m_networkRetryTimer = nullptr;
    QString m_cacheDir;

    QStringList m_refreshBackupAssetIds;
    int m_refreshBackupIndex = 0;
    bool m_slideActiveBeforeRefresh = false;

    int m_networkFailureStep = 0;
    int m_imageDownloadRetries = 0;
    bool m_wantsNetworkRetry = false;
};
