// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Window
import WxLens.App

Window {
    width: 800
    height: 700
    minimumWidth: 600
    minimumHeight: 450
    visible: true
    title: "WxLens crash report"
    color: themeManager.background
    CrashReportDialog {}
}
