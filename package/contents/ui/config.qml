/*
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrols as KQC2
import org.kde.plasma.wallpapers.immich

Kirigami.FormLayout {
    id: root

    twinFormLayouts: parentLayout ? parentLayout : []

    property alias formLayout: root
    property var configDialog
    property var wallpaperConfiguration
    property var parentLayout
    property string cfg_ServerUrl
    property int cfg_AuthMode
    property string cfg_ApiKey
    property string cfg_Email
    property string cfg_Password
    property int cfg_ShowMode
    property list<string> cfg_SelectedAlbumIds: []
    property list<string> cfg_SelectedTagIds: []
    property var selectedAlbumIds: []
    property var selectedTagIds: []
    property string filterText: ""
    property bool showSelectedOnly: false
    property int cfg_FillMode
    property alias cfg_Color: colorButton.color
    property int cfg_SlideInterval
    property int cfg_FadeDuration
    property int intervalHours: 0
    property int intervalMinutes: 0
    property int intervalSeconds: 30
    signal configurationChanged()

    function normalizedStringList(input) {
        if (!input) {
            return [];
        }
        return Array.from(input, item => String(item));
    }

    function toggleAlbum(albumId, checked) {
        const id = String(albumId);
        const list = selectedAlbumIds.slice();
        const i = list.indexOf(id);
        if (checked && i < 0) {
            list.push(id);
        } else if (!checked && i >= 0) {
            list.splice(i, 1);
        }
        selectedAlbumIds = list;
        cfg_SelectedAlbumIds = list;
        configurationChanged();
    }

    function toggleTag(tagId, checked) {
        const id = String(tagId);
        const list = selectedTagIds.slice();
        const i = list.indexOf(id);
        if (checked && i < 0) {
            list.push(id);
        } else if (!checked && i >= 0) {
            list.splice(i, 1);
        }
        selectedTagIds = list;
        cfg_SelectedTagIds = list;
        configurationChanged();
    }

    function filteredItems(items) {
        const query = filterText.trim().toLowerCase();
        const source = items || [];
        const out = [];
        const length = source.length || 0;
        const selectedLookup = {};
        const selected = cfg_ShowMode === 0 ? selectedAlbumIds : selectedTagIds;
        for (let j = 0; j < selected.length; ++j) {
            selectedLookup[String(selected[j])] = true;
        }

        for (let i = 0; i < length; ++i) {
            const item = source[i];
            const rawId = item ? (item["id"] !== undefined ? item["id"] : item.id) : "";
            const rawName = item ? (item["name"] !== undefined ? item["name"] : item.name) : "";
            const itemId = String(rawId || "");
            const itemName = String(rawName || "");
            if (!itemId.length && !itemName.length) {
                continue;
            }
            if (showSelectedOnly && !selectedLookup[itemId]) {
                continue;
            }
            if (query.length && itemName.toLowerCase().indexOf(query) < 0) {
                continue;
            }
            out.push({
                itemId: itemId,
                itemName: itemName
            });
        }

        return out;
    }

    function resetSelectorPosition() {
        if (selectorFlick) {
            selectorFlick.contentY = 0;
        }
    }

    function splitInterval() {
        const total = Math.max(30, Number(cfg_SlideInterval) || 30);
        intervalHours = Math.floor(total / 3600);
        const rem = total % 3600;
        intervalMinutes = Math.floor(rem / 60);
        intervalSeconds = rem % 60;
    }

    function updateIntervalFromParts() {
        let total = (intervalHours * 3600) + (intervalMinutes * 60) + intervalSeconds;
        if (total < 30) {
            total = 30;
            splitInterval();
        }
        if (cfg_SlideInterval !== total) {
            cfg_SlideInterval = total;
        }
    }

    function statusText() {
        if (backend.errorMessage.length > 0) {
            return backend.errorMessage;
        }
        if (backend.loading && backend.authenticated) {
            return "Connected. Updating results...";
        }
        if (backend.authenticated) {
            return "Login successful. " + (backend.matchCount || 0) + " photos match current settings.";
        }
        return " ";
    }

    function statusColor() {
        if (backend.errorMessage.length > 0) {
            return Kirigami.Theme.negativeTextColor;
        }
        return Kirigami.Theme.positiveTextColor;
    }

    onCfg_SelectedAlbumIdsChanged: selectedAlbumIds = normalizedStringList(cfg_SelectedAlbumIds)
    onCfg_SelectedTagIdsChanged: selectedTagIds = normalizedStringList(cfg_SelectedTagIds)
    onCfg_SlideIntervalChanged: splitInterval()
    onFilterTextChanged: resetSelectorPosition()
    Component.onCompleted: {
        selectedAlbumIds = normalizedStringList(cfg_SelectedAlbumIds);
        selectedTagIds = normalizedStringList(cfg_SelectedTagIds);
        splitInterval();
    }

    ImmichBackend {
        id: backend
        serverUrl: cfg_ServerUrl
        authMode: cfg_AuthMode
        apiKey: cfg_ApiKey
        email: cfg_Email
        password: cfg_Password
        showMode: cfg_ShowMode
        selectedAlbumIds: root.selectedAlbumIds
        selectedTagIds: root.selectedTagIds
        slideIntervalSeconds: cfg_SlideInterval
    }

    QQC2.TextField {
        Kirigami.FormData.label: "Immich server URL:"
        placeholderText: "https://photos.example.com"
        text: cfg_ServerUrl
        onEditingFinished: cfg_ServerUrl = text.trim()
    }

    RowLayout {
        Kirigami.FormData.label: "Authentication:"
        Layout.fillWidth: true

        QQC2.ComboBox {
            id: authModeCombo
            Layout.fillWidth: true
            model: [
                {
                    "label": "API key (recommended)",
                    "value": 0
                },
                {
                    "label": "Email + password",
                    "value": 1
                }
            ]
            textRole: "label"
            onActivated: cfg_AuthMode = model[currentIndex]["value"]
            Component.onCompleted: syncAuthMode()
            onModelChanged: syncAuthMode()

            function syncAuthMode() {
                for (let i = 0; i < model.length; i++) {
                    if (model[i]["value"] === cfg_AuthMode) {
                        currentIndex = i;
                        return;
                    }
                }
                currentIndex = 0;
            }
        }

        QQC2.ToolButton {
            icon.name: "help-contextual"
            display: QQC2.AbstractButton.IconOnly
            text: "Authentication help"
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: "API key mode is recommended. Create an API key in Immich user settings and paste it here. Required permissions: tag.read, album.read, asset.view, asset.read, asset.download, asset.view."
        }
    }

    QQC2.TextField {
        Kirigami.FormData.label: "API key:"
        visible: cfg_AuthMode === 0
        echoMode: QQC2.TextField.Password
        placeholderText: "Immich API key"
        text: cfg_ApiKey
        onEditingFinished: cfg_ApiKey = text.trim()
    }

    QQC2.TextField {
        Kirigami.FormData.label: "Email:"
        visible: cfg_AuthMode === 1
        placeholderText: "Immich login email"
        text: cfg_Email
        onEditingFinished: cfg_Email = text.trim()  
    }

    QQC2.TextField {
        Kirigami.FormData.label: "Password:"
        visible: cfg_AuthMode === 1
        echoMode: QQC2.TextField.Password
        text: cfg_Password
        onEditingFinished: cfg_Password = text
    }

    QQC2.Label {
        visible: cfg_AuthMode === 1
        text: "Warning: Password mode stores your password in wallpaper config. Prefer API key mode."
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        color: Kirigami.Theme.neutralTextColor
    }

    QQC2.Button {
        text: "Refresh lists"
        icon.name: "view-refresh-symbolic"
        onClicked: backend.refreshAlbums()
    }

    QQC2.ComboBox {
        id: showModeCombo
        Kirigami.FormData.label: "What to show:"
        model: [
            {
                "label": "Albums",
                "value": 0
            },
            {
                "label": "Tags",
                "value": 1
            },
            {
                "label": "Favorites",
                "value": 2
            },
            {
                "label": "All photos",
                "value": 3
            }
        ]
        textRole: "label"
        onActivated: cfg_ShowMode = model[currentIndex]["value"]
        Component.onCompleted: syncShowMode()
        onModelChanged: syncShowMode()
        onCurrentIndexChanged: {
            filterText = "";
            resetSelectorPosition();
        }

        function syncShowMode() {
            for (let i = 0; i < model.length; i++) {
                if (model[i]["value"] === cfg_ShowMode) {
                    currentIndex = i;
                    return;
                }
            }
            currentIndex = 0;
        }
    }

    QQC2.TextField {
        Kirigami.FormData.label: cfg_ShowMode === 0 ? "Filter albums:" : "Filter tags:"
        visible: cfg_ShowMode === 0 || cfg_ShowMode === 1
        placeholderText: "Type to filter..."
        text: filterText
        onTextChanged: filterText = text
    }

    QQC2.CheckBox {
        visible: cfg_ShowMode === 0 || cfg_ShowMode === 1
        text: "Show selected"
        checked: showSelectedOnly
        onToggled: showSelectedOnly = checked
    }

    QQC2.Label {
        text: statusText()
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        color: statusColor()
        minimumPixelSize: 12
        Layout.preferredHeight: implicitHeight
    }

    QQC2.Label {
        Kirigami.FormData.label: cfg_ShowMode === 0 ? "Albums:" : "Tags:"
        visible: (cfg_ShowMode === 0 || cfg_ShowMode === 1) && !backend.loading
            && ((cfg_ShowMode === 0 && backend.albums.length === 0) || (cfg_ShowMode === 1 && backend.tags.length === 0))
        text: "Enter credentials above, then refresh."
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    Item {
        id: selectorFrame
        Kirigami.FormData.label: cfg_ShowMode === 0 ? "Albums:" : "Tags:"
        visible: (cfg_ShowMode === 0 && root.filteredItems(backend.albums).length > 0)
            || (cfg_ShowMode === 1 && root.filteredItems(backend.tags).length > 0)
        Layout.fillWidth: true
        Layout.preferredWidth: 480
        implicitHeight: 260

        Rectangle {
            anchors.fill: parent
            radius: Kirigami.Units.smallSpacing
            color: Qt.rgba(1, 1, 1, 0.03)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.12)
        }

        Flickable {
            id: selectorFlick
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            contentWidth: width
            contentHeight: selectorColumn.implicitHeight

            Column {
                id: selectorColumn
                width: Math.max(0, selectorFlick.width - 8)
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: cfg_ShowMode === 0 ? root.filteredItems(backend.albums) : root.filteredItems(backend.tags)
                    delegate: QQC2.CheckBox {
                        required property var modelData
                        width: selectorColumn.width
                        text: modelData.itemName
                        checked: cfg_ShowMode === 0
                            ? root.selectedAlbumIds.indexOf(modelData.itemId) >= 0
                            : root.selectedTagIds.indexOf(modelData.itemId) >= 0
                        onToggled: {
                            if (cfg_ShowMode === 0) {
                                root.toggleAlbum(modelData.itemId, checked);
                            } else {
                                root.toggleTag(modelData.itemId, checked);
                            }
                        }
                    }
                }
            }

            QQC2.ScrollBar.vertical: QQC2.ScrollBar { }
        }
    }

    QQC2.Label {
        Kirigami.FormData.label: "Selection:"
        visible: cfg_ShowMode === 2 || cfg_ShowMode === 3
        text: cfg_ShowMode === 2
            ? "Favorites mode uses all favorite photos."
            : "All photos mode uses your full photo library."
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    QQC2.ComboBox {
        id: resizeComboBox
        Kirigami.FormData.label: "Positioning:"
        model: [
            {
                "label": "Scaled and cropped",
                "fillMode": Image.PreserveAspectCrop
            },
            {
                "label": "Scaled",
                "fillMode": Image.Stretch
            },
            {
                "label": "Scaled, keep proportions",
                "fillMode": Image.PreserveAspectFit
            },
            {
                "label": "Centered",
                "fillMode": Image.Pad
            },
            {
                "label": "Tiled",
                "fillMode": Image.Tile
            }
        ]
        textRole: "label"
        onActivated: cfg_FillMode = model[currentIndex]["fillMode"]
        Component.onCompleted: syncFillMode()
        onModelChanged: syncFillMode()

        function syncFillMode() {
            for (let i = 0; i < model.length; i++) {
                if (model[i]["fillMode"] === cfg_FillMode) {
                    currentIndex = i;
                    return;
                }
            }
        }
    }

    KQC2.ColorButton {
        id: colorButton
        Kirigami.FormData.label: "Background color:"
        dialogTitle: "Background color"
    }

    RowLayout {
        Kirigami.FormData.label: "Slide interval:"
        Layout.fillWidth: true

        QQC2.SpinBox {
            from: 0
            to: 24
            value: intervalHours
            editable: true
            onValueChanged: {
                intervalHours = value;
                root.updateIntervalFromParts();
            }
        }
        QQC2.Label { text: "hours" }

        QQC2.SpinBox {
            from: 0
            to: 59
            value: intervalMinutes
            editable: true
            onValueChanged: {
                intervalMinutes = value;
                root.updateIntervalFromParts();
            }
        }
        QQC2.Label { text: "minutes" }

        QQC2.SpinBox {
            from: 0
            to: 59
            value: intervalSeconds
            editable: true
            onValueChanged: {
                intervalSeconds = value;
                root.updateIntervalFromParts();
            }
        }
        QQC2.Label { text: "seconds" }
    }

    QQC2.SpinBox {
        Kirigami.FormData.label: "Fade duration (ms):"
        from: 0
        to: 5000
        stepSize: 100
        value: cfg_FadeDuration
        onValueChanged: cfg_FadeDuration = value
    }
}
