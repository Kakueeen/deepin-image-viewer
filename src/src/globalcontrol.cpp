// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalcontrol.h"
#include "types.h"
#include "imagedata/imagesourcemodel.h"
#include "utils/rotateimagehelper.h"

#include <QEvent>
#include <QThread>
#include <QDebug>
#include <QApplication>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logImageViewer)

static const int sc_SubmitInterval = 200;   // 图片变更提交定时间隔 200ms

/**
   @class GlobalControl
   @brief QML单例类，全局数据控制，用于提供图片展示过程中的数据、切换控制等。
 */

GlobalControl::GlobalControl(QObject *parent)
    : QObject(parent)
{
    qCDebug(logImageViewer) << "GlobalControl constructor entered.";
    sourceModel = new ImageSourceModel(this);
    viewSourceModel = new PathViewProxyModel(sourceModel, this);
    qCDebug(logImageViewer) << "ImageSourceModel and PathViewProxyModel initialized.";

    // 图片旋转完成后触发信息变更
    connect(RotateImageHelper::instance(), &RotateImageHelper::rotateImageFinished, this, [this](const QString &path, bool ret) {
        qCDebug(logImageViewer) << "Rotation finished for" << path << "success:" << ret;
        if (path == currentImage.source().toLocalFile()) {
            submitImageChangeImmediately();
            qCDebug(logImageViewer) << "Image change submitted immediately after rotation.";
        }
    });
    qCDebug(logImageViewer) << "Connected RotateImageHelper::rotateImageFinished signal.";
}

GlobalControl::~GlobalControl()
{
    qCDebug(logImageViewer) << "GlobalControl destructor entered.";
    submitImageChangeImmediately();
    qCDebug(logImageViewer) << "Image changes submitted on destruction.";
}

/**
   @return 返回全局的数据模型
 */
ImageSourceModel *GlobalControl::globalModel() const
{
    return sourceModel;
}

/**
   @return 返回用于大图展示的数据模型
 */
PathViewProxyModel *GlobalControl::viewModel() const
{
    return viewSourceModel;
}

/**
   @brief 设置当前显示的图片源为 \a source , 若图片源列表中无此图片，则不进行处理
 */
void GlobalControl::setCurrentSource(const QUrl &source)
{
    qCDebug(logImageViewer) << "Setting current source:" << source;
    if (currentImage.source() == source) {
        qCDebug(logImageViewer) << "Source unchanged, skipping update";
        return;
    }

    int index = sourceModel->indexForImagePath(source);
    if (-1 != index) {
        qCDebug(logImageViewer) << "Found source at index:" << index;
        setIndexAndFrameIndex(index, 0);
        qCDebug(logImageViewer) << "Updated index and frame index for new source.";
    } else {
        qCDebug(logImageViewer) << "Source not found in model";
    }
}

/**
   @return 返回当前设置的图片url地址
 */
QUrl GlobalControl::currentSource() const
{
    return currentImage.source();
}

/**
   @brief 设置当前展示的图片索引为 \a index
 */
void GlobalControl::setCurrentIndex(int index)
{
    qCDebug(logImageViewer) << "GlobalControl::setCurrentIndex() called, setting index to:" << index;
    setIndexAndFrameIndex(index, curFrameIndex);
}

/**
   @return 返回当前展示的图片索引
 */
int GlobalControl::currentIndex() const
{
    qCDebug(logImageViewer) << "GlobalControl::currentIndex() called, returning:" << curIndex;
    return curIndex;
}

/**
   @brief 设置当前展示的多页图图片索引为 \a frameIndex
 */
void GlobalControl::setCurrentFrameIndex(int frameIndex)
{
    qCDebug(logImageViewer) << "GlobalControl::setCurrentFrameIndex() called, setting frame index to:" << frameIndex;
    setIndexAndFrameIndex(curIndex, frameIndex);
}

/**
   @return 返回当前展示的多页图索引
 */
int GlobalControl::currentFrameIndex() const
{
    qCDebug(logImageViewer) << "GlobalControl::currentFrameIndex() called, returning:" << curFrameIndex;
    return curFrameIndex;
}

/**
   @return 返回当前图片总数
 */
int GlobalControl::imageCount() const
{
    qCDebug(logImageViewer) << "GlobalControl::imageCount() called, returning:" << sourceModel->rowCount();
    return sourceModel->rowCount();
}

/**
   @brief 设置当前图片旋转角度为 \a angle , 此变更不会立即更新，
        等待提交定时器结束后更新。
 */
void GlobalControl::setCurrentRotation(int angle)
{
    qCDebug(logImageViewer) << "Setting rotation angle:" << angle;
    if (imageRotation != angle) {
        qCDebug(logImageViewer) << "Rotation angle changed from" << imageRotation << "to" << angle;
        if (0 != (angle % 90)) {
            qCWarning(logImageViewer) << "Invalid rotation angle:" << angle << "- must be multiple of 90 degrees";
        }

        // 计算相较上一次是否需要交换宽高，angle 为 0 时特殊处理，不调整
        bool needSwap = angle && !!((angle - imageRotation) % 180);
        qCDebug(logImageViewer) << "Rotation change requires width/height swap:" << needSwap;

        imageRotation = angle;

        // 开始变更旋转文件缓存和参数设置前触发，用于部分前置操作更新
        Q_EMIT changeRotationCacheBegin();
        qCDebug(logImageViewer) << "Emitted changeRotationCacheBegin signal.";

        if (needSwap) {
            currentImage.swapWidthAndHeight();
            qCDebug(logImageViewer) << "Swapped image dimensions";
        }

        // 执行实际的旋转文件操作
        qCDebug(logImageViewer) << "Requesting image rotation:" << currentImage.source().toLocalFile() << "angle:" << angle;
        RotateImageHelper::instance()->rotateImageFile(currentImage.source().toLocalFile(), angle);
        // 保证更新界面旋转前刷新缓存，为0时同样通知，用以复位状态
        Q_EMIT requestRotateCacheImage();
        Q_EMIT currentRotationChanged();
        qCDebug(logImageViewer) << "Emitted requestRotateCacheImage and currentRotationChanged signals.";

        // 启动提交定时器
        submitTimer.start(sc_SubmitInterval, this);
        qCDebug(logImageViewer) << "Started rotation submit timer:" << sc_SubmitInterval << "ms";
    } else {
        qCDebug(logImageViewer) << "Rotation angle unchanged, skipping update.";
    }
}

/**
   @return 返回当前图片的旋转角度
 */
int GlobalControl::currentRotation()
{
    qCDebug(logImageViewer) << "GlobalControl::currentRotation() called, returning:" << imageRotation;
    return imageRotation;
}

/**
   @return 返回是否可切换到前一张图片
 */
bool GlobalControl::hasPreviousImage() const
{
    qCDebug(logImageViewer) << "GlobalControl::hasPreviousImage() called, returning:" << hasPrevious;
    return hasPrevious;
}

/**
   @return 返回是否可切换到后一张图片
 */
bool GlobalControl::hasNextImage() const
{
    qCDebug(logImageViewer) << "GlobalControl::hasNextImage() called, returning:" << hasNext;
    return hasNext;
}

/**
   @return 切换到前一张图片并返回是否切换成功
 */
bool GlobalControl::previousImage()
{
    qCDebug(logImageViewer) << "Attempting to navigate to previous image";
    submitImageChangeImmediately();
    qCDebug(logImageViewer) << "Image change submitted immediately before navigating previous.";

    if (hasPreviousImage()) {
        qCDebug(logImageViewer) << "Previous image available.";
        Q_ASSERT(sourceModel);
        if (Types::MultiImage == currentImage.type()) {
            if (curFrameIndex > 0) {
                qCDebug(logImageViewer) << "Navigating to previous frame in multi-image:" << (curFrameIndex - 1);
                setIndexAndFrameIndex(curIndex, curFrameIndex - 1);
                qCDebug(logImageViewer) << "Successfully navigated to previous frame.";
                return true;
            }
        }

        if (curIndex > 0) {
            qCDebug(logImageViewer) << "Navigating to previous image at index:" << (curIndex - 1);
            // 不确定前一张图片是何种类型，使用 INT_MAX 限定帧索引从尾部开始
            setIndexAndFrameIndex(curIndex - 1, INT_MAX);
            qCDebug(logImageViewer) << "Successfully navigated to previous image.";
            return true;
        }
    }

    qCDebug(logImageViewer) << "No previous image available";
    return false;
}

/**
   @return 切换到后一张图片并返回是否切换成功
 */
bool GlobalControl::nextImage()
{
    qCDebug(logImageViewer) << "Attempting to navigate to next image";
    submitImageChangeImmediately();
    qCDebug(logImageViewer) << "Image change submitted immediately before navigating next.";

    if (hasNextImage()) {
        qCDebug(logImageViewer) << "Next image available.";
        Q_ASSERT(sourceModel);
        if (Types::MultiImage == currentImage.type()) {
            if (curFrameIndex < currentImage.frameCount() - 1) {
                qCDebug(logImageViewer) << "Navigating to next frame in multi-image:" << (curFrameIndex + 1);
                setIndexAndFrameIndex(curIndex, curFrameIndex + 1);
                qCDebug(logImageViewer) << "Successfully navigated to next frame.";
                return true;
            }
        }

        if (curIndex < sourceModel->rowCount() - 1) {
            qCDebug(logImageViewer) << "Navigating to next image at index:" << (curIndex + 1);
            // 无论是否为多页图，均设置为0
            setIndexAndFrameIndex(curIndex + 1, 0);
            qCDebug(logImageViewer) << "Successfully navigated to next image.";
            return true;
        }
    }

    qCDebug(logImageViewer) << "No next image available";
    return false;
}

/**
   @return 切换到首张图片并返回是否切换成功
 */
bool GlobalControl::firstImage()
{
    qCDebug(logImageViewer) << "GlobalControl::firstImage() called.";
    submitImageChangeImmediately();

    Q_ASSERT(sourceModel);
    if (sourceModel->rowCount()) {
        qCDebug(logImageViewer) << "Setting index and frame index to 0,0.";
        setIndexAndFrameIndex(0, 0);
        return true;
    }
    qCDebug(logImageViewer) << "No images in model, cannot go to first image.";
    return false;
}

/**
   @return 切换到最后图片并返回是否切换成功
 */
bool GlobalControl::lastImage()
{
    qCDebug(logImageViewer) << "GlobalControl::lastImage() called.";
    submitImageChangeImmediately();

    Q_ASSERT(sourceModel);
    int count = sourceModel->rowCount();
    if (count) {
        qCDebug(logImageViewer) << "Model has images, count: " << count;
        int index = count - 1;
        int frameIndex = 0;

        if (Types::MultiImage == currentImage.type()) {
            qCDebug(logImageViewer) << "Current image is multi-page, setting frame index to last frame.";
            frameIndex = currentImage.frameCount() - 1;
        }

        setIndexAndFrameIndex(index, frameIndex);
        qCDebug(logImageViewer) << "Set index to " << index << " and frame index to " << frameIndex;
        return true;
    }
    qCDebug(logImageViewer) << "No images in model, cannot go to last image.";
    return false;
}

void GlobalControl::forceExit()
{
    qCDebug(logImageViewer) << "GlobalControl::forceExit() called, exiting application.";
    QApplication::exit(0);
    _Exit(0);
}

/**
   @brief 设置打开图片列表 \a filePaths ， 其中 \a openFile 是首个展示的图片路径，
    将更新全局数据源并发送状态变更信号
 */
void GlobalControl::setImageFiles(const QStringList &filePaths, const QString &openFile)
{
    qCDebug(logImageViewer) << "Setting image files, count:" << filePaths.size() << "initial file:" << openFile;
    Q_ASSERT(sourceModel);
    // 优先更新数据源
    sourceModel->setImageFiles(QUrl::fromStringList(filePaths));
    qCDebug(logImageViewer) << "Source model image files set.";

    int index = filePaths.indexOf(openFile);
    if (-1 == index || filePaths.isEmpty()) {
        index = 0;
        qCDebug(logImageViewer) << "Using default index 0";
    }

    setIndexAndFrameIndex(index, 0);

    // 更新图像信息，无论变更均更新
    if (currentImage.source() != openFile) {
        qCDebug(logImageViewer) << "Updating current image source to:" << openFile;
        currentImage.setSource(openFile);
    }
    Q_EMIT currentSourceChanged();
    qCDebug(logImageViewer) << "Emitted currentSourceChanged signal.";

    checkSwitchEnable();
    Q_EMIT imageCountChanged();
    qCDebug(logImageViewer) << "Emitted imageCountChanged signal.";

    // 更新视图展示模型
    viewSourceModel->resetModel(index, 0);
    qCDebug(logImageViewer) << "Image files set complete";
}

/**
   @brief 移除当前图片列表中文件路径为 \a removeImage 的图片，更新当前图片索引
 */
void GlobalControl::removeImage(const QUrl &removeImage)
{
    qCDebug(logImageViewer) << "Removing image:" << removeImage;
    if (0 != currentRotation()) {
        qCDebug(logImageViewer) << "Resetting rotation before removal";
        setCurrentRotation(0);
        submitTimer.stop();
        qCDebug(logImageViewer) << "Submit timer stopped.";
    }

    // 移除当前图片，默认将后续图片前移，currentIndex将不会变更，手动提示更新
    bool atEnd = (curIndex >= sourceModel->rowCount() - 1);
    qCDebug(logImageViewer) << "Image is at end of list:" << atEnd;

    // 模型更新后将自动触发QML切换当前显示图片
    sourceModel->removeImage(removeImage);
    qCDebug(logImageViewer) << "Image removed from source model.";

    // NOTE：viewModel依赖源数据模型更新
    if (removeImage == currentImage.source()) {
        qCDebug(logImageViewer) << "Removing current image from view model";
        viewModel()->deleteCurrent();
    }

    if (!atEnd) {
        qCDebug(logImageViewer) << "Current image was not at the end of the list. Updating to next image in list.";
        // 需要提示的情况下不会越界
        const QUrl image = sourceModel->data(sourceModel->index(curIndex), Types::ImageUrlRole).toUrl();
        qCDebug(logImageViewer) << "Updating current image to next in list:" << image;
        currentImage.setSource(image);

        setIndexAndFrameIndex(curIndex, 0);
        Q_EMIT currentSourceChanged();
        Q_EMIT currentIndexChanged();
        qCDebug(logImageViewer) << "Emitted currentSourceChanged and currentIndexChanged signals.";
    } else if (/*atEnd &&*/ (0 != sourceModel->rowCount())) {
        qCDebug(logImageViewer) << "Current image was at the end of the list, and there are still images. Updating to previous image.";
        // 删除的尾部文件且仍有数据，更新当前文件信息
        const QUrl image = sourceModel->data(sourceModel->index(curIndex - 1), Types::ImageUrlRole).toUrl();
        qCDebug(logImageViewer) << "Updating current image to previous in list:" << image;
        currentImage.setSource(image);

        setIndexAndFrameIndex(curIndex - 1, INT_MAX);
        Q_EMIT currentSourceChanged();
        Q_EMIT currentIndexChanged();
        qCDebug(logImageViewer) << "Emitted currentSourceChanged and currentIndexChanged signals.";
    } else {
        qCDebug(logImageViewer) << "No images left in the model after removal.";
    }

    checkSwitchEnable();
    Q_EMIT imageCountChanged();
    qCDebug(logImageViewer) << "Image removal complete";
}

/**
   @brief 图片重命名后更新数据，路径由 \a oldName 更新为 \a newName 。
 */
void GlobalControl::renameImage(const QUrl &oldName, const QUrl &newName)
{
    qCDebug(logImageViewer) << "Renaming image from" << oldName << "to" << newName;
    int index = sourceModel->indexForImagePath(oldName);
    if (-1 != index) {
        qCDebug(logImageViewer) << "Image found at index: " << index;
        submitImageChangeImmediately();

        sourceModel->setData(sourceModel->index(index), newName, Types::ImageUrlRole);
        viewSourceModel->setData(viewSourceModel->index(viewSourceModel->currentIndex()), newName, Types::ImageUrlRole);
        qCDebug(logImageViewer) << "Image data updated in source and view models.";

        if (oldName == currentImage.source()) {
            qCDebug(logImageViewer) << "Updating current image source after rename";
            // 强制刷新，避免出现重命名为已缓存的删除图片
            currentImage.setSource(newName);
            currentImage.reloadData();

            setIndexAndFrameIndex(curIndex, 0);
            Q_EMIT currentSourceChanged();
            Q_EMIT currentIndexChanged();
            qCDebug(logImageViewer) << "Emitted currentSourceChanged and currentIndexChanged signals.";
        }
    } else {
        qCDebug(logImageViewer) << "Image not found in model for rename";
    }
}

/**
   @brief 提交当前图片的变更到图片文件，将触发文件重新写入磁盘
   @warning 在执行切换、删除、重命名等操作前，需手动执行提交当前图片变更信息的操作
 */
void GlobalControl::submitImageChangeImmediately()
{
    qCDebug(logImageViewer) << "Submitting image changes immediately";
    submitTimer.stop();
    int rotation = currentRotation();
    if (0 == rotation) {
        qCDebug(logImageViewer) << "No rotation changes to submit";
        return;
    }

    rotation = rotation % 360;
    if (0 != rotation) {
        qCDebug(logImageViewer) << "Submitting rotation:" << rotation << "for image:" << currentImage.source().toLocalFile();
        // 请求更新图片，同步图片旋转状态到文件中，将覆写文件
        Q_EMIT requestRotateImage(currentImage.source().toLocalFile(), rotation);
        qCDebug(logImageViewer) << "Emitted requestRotateImage signal.";
    }

    // 重置状态
    setCurrentRotation(0);
    qCDebug(logImageViewer) << "Image changes submitted";
}

/**
   @return 返回是否允许使用多线程处理图像数据
   @warning 在部分平台多线程可能出现问题，使用多线程的线程计数限制，低于2逻辑线程将不使用多线程处理
 */
bool GlobalControl::enableMultiThread()
{
    qCDebug(logImageViewer) << "GlobalControl::enableMultiThread() called.";
    static const int sc_MaxThreadCountLimit = 2;
    return bool(QThread::idealThreadCount() > sc_MaxThreadCountLimit);
}

/**
   @brief 响应定时器事件，此处用于延时更新图片旋转
 */
void GlobalControl::timerEvent(QTimerEvent *event)
{
    qCDebug(logImageViewer) << "GlobalControl::timerEvent() called.";
    if (submitTimer.timerId() == event->timerId()) {
        qCDebug(logImageViewer) << "Submit timer timed out.";
        submitTimer.stop();
        submitImageChangeImmediately();
        qCDebug(logImageViewer) << "Submit timer stopped and image changes submitted.";
    }
}

/**
   @brief 根据当前展示图片索引判断是否允许切换前后图片
 */
void GlobalControl::checkSwitchEnable()
{
    qCDebug(logImageViewer) << "GlobalControl::checkSwitchEnable() called.";
    Q_ASSERT(sourceModel);
    bool previous = (curIndex > 0 || curFrameIndex > 0);
    bool next = (curIndex < (sourceModel->rowCount() - 1) || curFrameIndex < (currentImage.frameCount() - 1));
    qCDebug(logImageViewer) << "Can go previous: " << previous << ", Can go next: " << next;

    if (previous != hasPrevious) {
        hasPrevious = previous;
        Q_EMIT hasPreviousImageChanged();
        qCDebug(logImageViewer) << "hasPreviousImage changed to " << hasPrevious << ", emitting hasPreviousImageChanged.";
    }
    if (next != hasNext) {
        hasNext = next;
        Q_EMIT hasNextImageChanged();
        qCDebug(logImageViewer) << "hasNextImage changed to " << hasNext << ", emitting hasNextImageChanged.";
    }
}

/**
   @brief 根据图片索引 \a index 和帧索引 \a frameIndex 设置当前展示的图片
    会调整索引位置在允许范围内，可通过传入 \a frameIndex 为 INT_MAX 限制从尾帧开始读取图片
   @note \a index 和 \a frameIndex 均变更时需调用此函数，分别设置时会导致视图模型的前后计算位置不正确
 */
void GlobalControl::setIndexAndFrameIndex(int index, int frameIndex)
{
    qCDebug(logImageViewer) << "Setting index:" << index << "frame index:" << frameIndex;
    int validIndex = qBound(0, index, imageCount() - 1);
    if (this->curIndex != validIndex) {
        qCDebug(logImageViewer) << "Current index changed from " << this->curIndex << " to " << validIndex << ", submitting image change.";
        submitImageChangeImmediately();

        // 更新图像信息，无论变更均更新
        QUrl image = sourceModel->data(sourceModel->index(validIndex), Types::ImageUrlRole).toUrl();
        qCDebug(logImageViewer) << "Updating current image to:" << image;
        currentImage.setSource(image);
        Q_EMIT currentSourceChanged();
        qCDebug(logImageViewer) << "Emitted currentSourceChanged signal.";

        this->curIndex = index;
        Q_EMIT currentIndexChanged();
        qCDebug(logImageViewer) << "Emitted currentIndexChanged signal.";
    }

    int validFrameIndex = qBound(0, frameIndex, qMax(0, currentImage.frameCount() - 1));
    if (this->curFrameIndex != validFrameIndex) {
        qCDebug(logImageViewer) << "Current frame index changed from " << this->curFrameIndex << " to " << validFrameIndex << ", submitting image change.";
        submitImageChangeImmediately();

        this->curFrameIndex = validFrameIndex;
        Q_EMIT currentFrameIndexChanged();
    }

    checkSwitchEnable();

    // 更新视图模型
    viewSourceModel->setCurrentSourceIndex(curIndex, curFrameIndex);
    qCDebug(logImageViewer) << "Index and frame index update complete";
}
