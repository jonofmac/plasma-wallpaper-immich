/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.wallpapers.immich

WallpaperItem {
    id: root

    loading: backend.loading
    contextualActions: [nextImageAction, previousImageAction]
    property bool frontIsA: true
    property string currentSource: ""
    property string pendingSource: ""
    property int fadeDurationMs: {
        const value = Number(root.configuration.FadeDuration);
        if (Number.isNaN(value)) {
            return 1200;
        }
        return Math.max(0, value);
    }

    function applySourceNow(sourceUrl) {
        currentSource = sourceUrl;
        pendingSource = "";
        if (frontIsA) {
            imageA.source = sourceUrl;
            imageA.opacity = sourceUrl.length > 0 ? 1 : 0;
            imageB.opacity = 0;
        } else {
            imageB.source = sourceUrl;
            imageB.opacity = sourceUrl.length > 0 ? 1 : 0;
            imageA.opacity = 0;
        }
    }

    function queueTransition(sourceUrl) {
        if (sourceUrl.length === 0) {
            currentSource = "";
            pendingSource = "";
            imageA.opacity = 0;
            imageB.opacity = 0;
            return;
        }

        if (sourceUrl === currentSource || sourceUrl === pendingSource) {
            return;
        }

        if (currentSource.length === 0) {
            applySourceNow(sourceUrl);
            return;
        }

        pendingSource = sourceUrl;
        const incoming = frontIsA ? imageB : imageA;
        incoming.source = sourceUrl;
        incoming.opacity = 0;

        if (incoming.status === Image.Ready) {
            startFadeIfPossible();
        }
    }

    function startFadeIfPossible() {
        if (pendingSource.length === 0 || fadeAnimation.running) {
            return;
        }
        if (fadeDurationMs <= 0) {
            applySourceNow(pendingSource);
            return;
        }
        fadeAnimation.restart();
    }

    ImmichBackend {
        id: backend
        serverUrl: root.configuration.ServerUrl
        authMode: root.configuration.AuthMode
        apiKey: root.configuration.ApiKey
        email: root.configuration.Email
        password: root.configuration.Password
        showMode: root.configuration.ShowMode
        selectedAlbumIds: root.configuration.SelectedAlbumIds
        selectedTagIds: root.configuration.SelectedTagIds
        slideIntervalSeconds: root.configuration.SlideInterval
    }

    PlasmaCore.Action {
        id: previousImageAction
        text: "Previous Image"
        icon.name: "go-previous"
        enabled: backend.canNavigate
        onTriggered: {
            if (typeof backend.previousImage === "function") {
                backend.previousImage();
            }
        }
    }

    PlasmaCore.Action {
        id: nextImageAction
        text: "Next Image"
        icon.name: "go-next"
        enabled: backend.canNavigate
        onTriggered: {
            if (typeof backend.nextImage === "function") {
                backend.nextImage();
            }
        }
    }

    Connections {
        target: backend
        function onLocalUrlChanged() {
            root.queueTransition(backend.localUrl);
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.configuration.Color
        visible: root.currentSource.length === 0 && root.pendingSource.length === 0
    }

    Image {
        id: imageA
        anchors.fill: parent
        asynchronous: true
        cache: false
        autoTransform: true
        fillMode: root.configuration.FillMode
        sourceSize: Qt.size(root.width * Screen.devicePixelRatio, root.height * Screen.devicePixelRatio)

        onStatusChanged: {
            if (!root.frontIsA && root.pendingSource.length > 0 && status === Image.Ready) {
                root.startFadeIfPossible();
            }
        }
    }

    Image {
        id: imageB
        anchors.fill: parent
        asynchronous: true
        cache: false
        autoTransform: true
        fillMode: root.configuration.FillMode
        sourceSize: Qt.size(root.width * Screen.devicePixelRatio, root.height * Screen.devicePixelRatio)

        onStatusChanged: {
            if (root.frontIsA && root.pendingSource.length > 0 && status === Image.Ready) {
                root.startFadeIfPossible();
            }
        }
    }

    ParallelAnimation {
        id: fadeAnimation
        NumberAnimation {
            target: root.frontIsA ? imageA : imageB
            property: "opacity"
            to: 0
            duration: root.fadeDurationMs
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: root.frontIsA ? imageB : imageA
            property: "opacity"
            to: 1
            duration: root.fadeDurationMs
            easing.type: Easing.InOutQuad
        }
        onFinished: {
            root.currentSource = root.pendingSource;
            root.pendingSource = "";
            root.frontIsA = !root.frontIsA;
        }
    }

    Component.onCompleted: {
        if (backend.localUrl.length > 0) {
            applySourceNow(backend.localUrl);
        }
    }
}
