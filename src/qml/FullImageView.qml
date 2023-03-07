
// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.11
import QtQuick.Controls 2.4
import QtQuick.Window 2.11
import QtGraphicalEffects 1.0
import org.deepin.dtk 1.0

Item {
    id: fullThumbnail

    // 鼠标是否进入当前的视图
    property bool isEnterCurrentView: true
    // 是否标题栏和底栏需要隐藏(仅判断普通模式)
    property bool needBarHideInNormalMode: false

    anchors.fill: parent

    // 切换标题栏和工具栏显示状态
    function switchTopAndBottomBarState() {
        // 判断当前标题栏、工具栏处于是否隐藏模式下
        if (needBarHideInNormalMode || window.isFullScreen) {
            var curRectY = thumbnailViewBackGround.y
            //判断当前标题栏、工具栏是否已隐藏
            if (window.height <= curRectY) {
                hideTopTitleAnimation.stop()
                hideBottomAnimation.stop()

                // 全屏下不展示标题栏
                if (!window.isFullScreen) {
                    showTopTitleAnimation.start()
                }
                showBottomAnimation.start()
            } else {
                showTopTitleAnimation.stop()
                showBottomAnimation.stop()

                hideTopTitleAnimation.start()
                hideBottomAnimation.start()
            }
        }
    }

    //判断工具栏和标题栏的显示隐藏
    function animationAll() {
        if (GStatus.animationBlock) {
            return
        }

        // 打开界面不计算标题栏显隐
        if (stackView.currentWidgetIndex === 0) {
            return
        }

        // 根据当前不同捕获行为获取光标值
        var mouseX = imageViewerArea.usingCapture ? imageViewerArea.captureX : imageViewerArea.mouseX
        var mouseY = imageViewerArea.usingCapture ? imageViewerArea.captureY : imageViewerArea.mouseY

        // 判断光标是否离开了窗口
        var cursorInWidnow = mouseX >= 0 && mouseX <= window.width && mouseY >= 0
                && mouseY <= window.height
        // 显示图像的像素高度
        var viewImageHeight = window.width * (fileControl.getCurrentImageHeight() / fileControl.getCurrentImageWidth())
        // 工具栏显示的热区高度，窗口高度 - 工具栏距底部高度(工具栏高 70px + 边距 10px)
        var bottomHotspotHeight = window.height - GStatus.showBottomY

        if (window.isFullScreen) {
            // 全屏时特殊处理
            if (mouseY > bottomHotspotHeight) {
                showBottomAnimation.start()
            } else {
                // 隐藏动画前结束弹出动画
                showBottomAnimation.stop()
                showTopTitleAnimation.stop()

                hideBottomAnimation.start()
                hideTopTitleAnimation.start()
            }
        } else {
            // 判断是否弹出标题栏和缩略图栏
            var needShowTopBottom = false
            if (stackPage != 0 && ((window.height <= GStatus.minHideHeight
                                    || window.width <= GStatus.minWidth)
                                   && (mouseY <= bottomHotspotHeight)
                                   && (mouseY >= titleRect.height))) {
                needShowTopBottom = false
            } else if (imageViewer.currentScale
                       <= (1.0 * (window.height - titleRect.height * 2) / window.height)) {
                // 缩放率小于(允许显示高度/窗口高度)的不会超过工具/标题栏
                needShowTopBottom = true
            } else if ((viewImageHeight * imageViewer.currentScale)
                       <= (window.height - titleRect.height * 2)) {
                // 缩放范围高度未超过显示范围高度限制时时，不会隐藏工具/标题栏，根据高度而非宽度计算
                needShowTopBottom = true
            } else if (cursorInWidnow
                       && ((mouseY > bottomHotspotHeight && mouseY <= height)
                           || (0 < mouseY && mouseY < titleRect.height))) {
                // 当缩放范围超过工具/标题栏且光标在工具/标题栏范围，显示工具/标题栏
                needShowTopBottom = true
            } else {
                needShowTopBottom = false
            }

            if (needShowTopBottom) {
                showBottomAnimation.start()
                showTopTitleAnimation.start()
            } else {
                // 隐藏动画前结束弹出动画
                showBottomAnimation.stop()
                showTopTitleAnimation.stop()

                hideBottomAnimation.start()
                hideTopTitleAnimation.start()
            }
            needBarHideInNormalMode = !needShowTopBottom
        }

        // 光标不在切换按钮纵向判断的热区(处于标题栏/工具栏区域)时，隐藏左右切换按钮
        // 判断是否弹出图片切换按钮
        var needShowLeftRightBtn = false
        if (titleRect.height < mouseY && mouseY < bottomHotspotHeight
                && isEnterCurrentView && cursorInWidnow) {
            if (mouseX >= window.width - GStatus.switchImageHotspotWidth
                    && mouseX <= window.width) {
                // 光标处于切换下一张按钮区域
                needShowLeftRightBtn = true
            } else if (mouseX <= GStatus.switchImageHotspotWidth
                       && mouseX >= 0) {
                // 光标处于切换上一张按钮区域
                needShowLeftRightBtn = true
            }
        }

        if (needShowLeftRightBtn) {
            showLeftButtonAnimation.start()
            showRightButtonAnimation.start()
        } else {
            // 隐藏动画前结束弹出动画
            showLeftButtonAnimation.stop()
            showRightButtonAnimation.stop()

            hideLeftButtonAnimation.start()
            hideRightButtonAnimation.start()
        }
    }

    //判断工具栏和标题栏的显示隐藏
    function changeSizeMoveAll() {
        showBottomAnimation.stop()
        showTopTitleAnimation.stop()
        hideBottomAnimation.stop()
        hideTopTitleAnimation.stop()
        showRightButtonAnimation.stop()
        hideRightButtonAnimation.stop()
        if (window.isFullScreen) {
            if (imageViewerArea.mouseY > height - 100) {
                thumbnailViewBackGround.y = window.height - GStatus.showBottomY
            } else {
                thumbnailViewBackGround.y = window.height
                titleRect.y = -50
            }
        } else if (currentWidgetIndex != 0
                   && ((window.height <= GStatus.minHideHeight
                        || window.width <= GStatus.minWidth)
                       && (imageViewerArea.mouseY <= height - 100)
                       && imageViewerArea.mouseY >= titleRect.height)) {
            thumbnailViewBackGround.y = window.height
            titleRect.y = -50
        } else if (imageViewerArea.mouseY > height - 100
                   || imageViewerArea.mouseY < titleRect.height
                   || (imageViewer.currentScale <= 1.0
                       * (window.height - titleRect.height * 2) / window.height)) {
            thumbnailViewBackGround.y = window.height - GStatus.showBottomY
            titleRect.y = 0
        } else {
            thumbnailViewBackGround.y = window.height
            titleRect.y = -50
        }

        if (imageViewerArea.mouseX <= 100
                && imageViewerArea.mouseX <= window.width && isEnterCurrentView) {
            floatLeftButton.x = 20
            floatRightButton.x = parent.width - 70
        } else if (imageViewerArea.mouseX >= window.width - 100
                   && imageViewerArea.mouseX >= 0 && isEnterCurrentView) {
            floatLeftButton.x = 20
            floatRightButton.x = parent.width - 70
        } else {
            floatLeftButton.x = -50
            floatRightButton.x = parent.width
        }
    }

    function slotShowFullScreen() {
        thumbnailViewBackGround.y = Screen.height
        floatRightButton.x = Screen.width
    }

    function slotMaxWindow() {
        thumbnailViewBackGround.y = Screen.height
        floatRightButton.x = Screen.width
    }

    onHeightChanged: {
        changeSizeMoveAll()
    }

    onWidthChanged: {
        changeSizeMoveAll()
    }

    //左右按钮隐藏动画
    NumberAnimation {
        id: hideLeftButtonAnimation

        target: floatLeftButton
        from: floatLeftButton.x
        to: -50
        property: "x"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: showLeftButtonAnimation

        target: floatLeftButton
        from: floatLeftButton.x
        to: 20
        property: "x"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: hideRightButtonAnimation

        target: floatRightButton
        from: floatRightButton.x
        to: parent.width
        property: "x"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: showRightButtonAnimation

        target: floatRightButton
        from: floatRightButton.x
        to: parent.width - 70
        property: "x"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    //工具栏动画和标题栏动画
    NumberAnimation {
        id: hideBottomAnimation

        target: thumbnailViewBackGround
        from: thumbnailViewBackGround.y
        to: window.height
        property: "y"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: hideTopTitleAnimation

        target: titleRect
        from: titleRect.y
        to: -50
        property: "y"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: showBottomAnimation

        target: thumbnailViewBackGround
        from: thumbnailViewBackGround.y
        to: window.height - GStatus.showBottomY
        property: "y"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: showTopTitleAnimation
        target: titleRect
        from: titleRect.y
        to: 0
        property: "y"
        duration: 200
        easing.type: Easing.InOutQuad
    }

    ImageViewer {
        id: imageViewer
        anchors.fill: parent
    }

    Connections {
        target: imageViewer
        onSigWheelChange: {
            animationAll()
        }
    }

    FloatingButton {
        id: floatLeftButton

        checked: false
        enabled: GControl.hasPreviousImage
        visible: enabled
        anchors {
            top: parent.top
            topMargin: GStatus.titleHeight + (parent.height - GStatus.titleHeight - GStatus.showBottomY) / 2
        }
        width: 50
        height: 50
        icon.name: "icon_previous"

        onClicked: GControl.previousImage()
    }

    FloatingButton {
        id: floatRightButton

        checked: false
        enabled: GControl.hasNextImage
        visible: enabled
        anchors {
            top: parent.top
            topMargin: GStatus.titleHeight + (parent.height - GStatus.titleHeight - GStatus.showBottomY) / 2
        }
        width: 50
        height: 50
        icon.name: "icon_next"

        onClicked: GControl.nextImage()
    }

    MouseArea {
        anchors.fill: imageViewer
        id: imageViewerArea
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true

        property bool usingCapture: false // 是否使用定时捕获光标位置
        property int captureX: 0 // 当前的光标X坐标值
        property int captureY: 0 // 当前的光标Y坐标值

        onMouseYChanged: {
            animationAll()
            mouse.accepted = false
        }

        onEntered: {
            isEnterCurrentView = true
            animationAll()
        }

        onExited: {
            isEnterCurrentView = false
            animationAll()

            // 当光标移出当前捕获范围时触发(不一定移出了窗口)
            cursorTool.setCaptureCursor(true)
            imageViewerArea.usingCapture = true
        }

        Connections {
            target: cursorTool
            onCursorPosChanged: {
                if (imageViewerArea.usingCapture) {
                    var pos = mapFromGlobal(x, y)
                    imageViewerArea.captureX = pos.x
                    imageViewerArea.captureY = pos.y
                    // 根据光标位置计算工具、标题、侧边栏的收缩弹出
                    animationAll()

                    // 若光标已移出界面，停止捕获光标位置
                    var cursorInWidnow = pos.x >= 0 && pos.x <= window.width
                            && pos.y >= 0 && pos.y <= window.height
                    if (!cursorInWidnow) {
                        cursorTool.setCaptureCursor(false)
                        imageViewerArea.usingCapture = false
                    }
                }
            }
        }
    }

    FloatingPanel {
        id: thumbnailViewBackGround

        anchors.right: parent.right
        anchors.rightMargin: (parent.width - width) / 2
        // 根据拓展的列表宽度计算, 20px为工具栏和主窗口间的间距 2x10px
        width: parent.width - 20 < thumbnailListView.btnContentWidth + thumbnailListView.listContentWidth
               ? parent.width - 20 : thumbnailListView.btnContentWidth + thumbnailListView.listContentWidth
        height: 70
    }

    ThumbnailListView {
        id: thumbnailListView

        anchors.fill: thumbnailViewBackGround
    }

    //浮动提示框
    FloatingNotice {
        id: floatLabel

        visible: false
        anchors.bottom: parent.bottom
        anchors.bottomMargin: thumbnailViewBackGround.height + GStatus.floatMargin
        anchors.left: parent.left
        anchors.leftMargin: parent.width / 2 - 50
        opacity: 0.7

        Timer {
            interval: 1500
            running: parent.visible
            repeat: false
            onTriggered: {
                parent.visible = false
            }
        }
    }

    Component.onCompleted: {
        animationAll()
    }
}
